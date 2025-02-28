//===-- GlobalDCE.cpp - DCE unreachable internal functions ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This transform is designed to eliminate unreachable internal globals from the
// program.  It uses an aggressive algorithm, searching out globals that are
// known to be alive.  After it finds all of the globals which are needed, it
// deletes whatever is left over.  This allows it to delete recursive chunks of
// the program which are unreachable.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TypeMetadataUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/CtorUtils.h"
#include "llvm/Transforms/Utils/GlobalStatus.h"
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "globaldce"

static cl::opt<bool>
    ClEnableVFE("enable-vfe", cl::Hidden, cl::init(true), cl::ZeroOrMore,
                cl::desc("Enable virtual function elimination"));

STATISTIC(NumAliases  , "Number of global aliases removed");
STATISTIC(NumFunctions, "Number of functions removed");
STATISTIC(NumIFuncs,    "Number of indirect functions removed");
STATISTIC(NumVariables, "Number of global variables removed");
STATISTIC(NumVFuncs,    "Number of virtual functions removed");

namespace {
  class GlobalDCELegacyPass : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    GlobalDCELegacyPass() : ModulePass(ID) {
      initializeGlobalDCELegacyPassPass(*PassRegistry::getPassRegistry());
    }

    // run - Do the GlobalDCE pass on the specified module, optionally updating
    // the specified callgraph to reflect the changes.
    //
    bool runOnModule(Module &M) override {
      if (skipModule(M))
        return false;

      // We need a minimally functional dummy module analysis manager. It needs
      // to at least know about the possibility of proxying a function analysis
      // manager.
      FunctionAnalysisManager DummyFAM;
      ModuleAnalysisManager DummyMAM;
      DummyMAM.registerPass(
          [&] { return FunctionAnalysisManagerModuleProxy(DummyFAM); });

      auto PA = Impl.run(M, DummyMAM);
      return !PA.areAllPreserved();
    }

  private:
    GlobalDCEPass Impl;
  };
}

char GlobalDCELegacyPass::ID = 0;
INITIALIZE_PASS(GlobalDCELegacyPass, "globaldce",
                "Dead Global Elimination", false, false)

// Public interface to the GlobalDCEPass.
ModulePass *llvm::createGlobalDCEPass() {
  return new GlobalDCELegacyPass();
}

/// Returns true if F is effectively empty.
static bool isEmptyFunction(Function *F) {
  BasicBlock &Entry = F->getEntryBlock();
  for (auto &I : Entry) {
    if (I.isDebugOrPseudoInst())
      continue;
    if (auto *RI = dyn_cast<ReturnInst>(&I))
      return !RI->getReturnValue();
    break;
  }
  return false;
}

//Return which globalVariable depends from a given Instruction
//There are 3 cases:
//	- IF it's a non-volatile store to inside an "internal" GV, return it
//	- othwerwise return the containing function for instructions
//		that may in other way modify the state/execution
//	- or return nullptr (in this case it will be calculated as union of the users's dependencies
static GlobalValue* dependentGlobalVariable(Instruction* I)
{
  if (isa<StoreInst>(I) && !dyn_cast<StoreInst>(I)->isVolatile()) {
    Value* currOp = I->getOperand(1);
    while (!isa<GlobalValue>(currOp)) {
      if (isa<ConstantExpr>(currOp) && (dyn_cast<ConstantExpr>(currOp)->isCast() ||
        (isa<GEPOperator>(currOp) && cast<GEPOperator>(currOp)->isInBounds() && cast<GEPOperator>(currOp)->hasAllConstantIndices())))
        currOp = dyn_cast<ConstantExpr>(currOp)->getOperand(0);
      else if (isa<GetElementPtrInst>(currOp) && dyn_cast<GetElementPtrInst>(currOp)->isInBounds())
        currOp = dyn_cast<GetElementPtrInst>(currOp)->getOperand(0);
      else if (isa<BitCastInst>(currOp))
        currOp = dyn_cast<BitCastInst>(currOp)->getOperand(0);
      else
        break;
    }

    if (GlobalValue* GV = dyn_cast<GlobalValue>(currOp))
      if (GV->isDiscardableIfUnused())
        return GV;
  }
  if (I->isTerminator() || isa<CallInst>(I) || isa<InvokeInst>(I) || isa<PHINode>(I) ||
                (isa<LoadInst>(I) && dyn_cast<LoadInst>(I)->isVolatile()) ||
                (isa<StoreInst>(I)))		//StoreInst not catched by the previous case should fail
    return I->getFunction();

  return nullptr;
}
/// Compute the set of GlobalValue that depends from V.
/// The recursion stops as soon as a GlobalValue is met.
void GlobalDCEPass::ComputeDependencies(Value *V,
                                        SmallPtrSetImpl<GlobalValue *> &Deps) {
  if (auto *I = dyn_cast<Instruction>(V)) {
    auto Where = InstructionDependenciesCache.find(I);
    if (Where != InstructionDependenciesCache.end()) {
      auto const &K = Where->second;
      Deps.insert(K.begin(), K.end());
    } else {
      SmallPtrSetImpl<GlobalValue *> &LocalDeps = InstructionDependenciesCache[I];
      GlobalValue* dependentGV = dependentGlobalVariable(I);
      if (dependentGV)
        LocalDeps.insert(dependentGV);

      if (!isa<PHINode>(I))
        for (User *CEUser : I->users())
          ComputeDependencies(CEUser, LocalDeps);

      Deps.insert(LocalDeps.begin(), LocalDeps.end());
    }
  } else if (auto *GV = dyn_cast<GlobalValue>(V)) {
    Deps.insert(GV);
  } else if (auto *CE = dyn_cast<Constant>(V)) {
    // Avoid walking the whole tree of a big ConstantExprs multiple times.
    auto Where = ConstantDependenciesCache.find(CE);
    if (Where != ConstantDependenciesCache.end()) {
      auto const &K = Where->second;
      Deps.insert(K.begin(), K.end());
    } else {
      SmallPtrSetImpl<GlobalValue *> &LocalDeps = ConstantDependenciesCache[CE];
      for (User *CEUser : CE->users())
        ComputeDependencies(CEUser, LocalDeps);
      Deps.insert(LocalDeps.begin(), LocalDeps.end());
    }
  }
}

void GlobalDCEPass::UpdateGVDependencies(GlobalValue &GV) {
  SmallPtrSet<GlobalValue *, 8> Deps;
  for (User *User : GV.users())
    ComputeDependencies(User, Deps);
  Deps.erase(&GV); // Remove self-reference.
  for (GlobalValue *GVU : Deps) {
    // If this is a dep from a vtable to a virtual function, and we have
    // complete information about all virtual call sites which could call
    // though this vtable, then skip it, because the call site information will
    // be more precise.
    if (VFESafeVTables.count(GVU) && isa<Function>(&GV)) {
      LLVM_DEBUG(dbgs() << "Ignoring dep " << GVU->getName() << " -> "
                        << GV.getName() << "\n");
      continue;
    }
    GVDependencies[GVU].insert(&GV);
  }
}

/// Mark Global value as Live
void GlobalDCEPass::MarkLive(GlobalValue &GV,
                             SmallVectorImpl<GlobalValue *> *Updates) {
  auto const Ret = AliveGlobals.insert(&GV);
  if (!Ret.second)
    return;

  if (Updates)
    Updates->push_back(&GV);
  if (Comdat *C = GV.getComdat()) {
    for (auto &&CM : make_range(ComdatMembers.equal_range(C))) {
      MarkLive(*CM.second, Updates); // Recursion depth is only two because only
                                     // globals in the same comdat are visited.
    }
  }
}

void GlobalDCEPass::ScanVTables(Module &M) {
  SmallVector<MDNode *, 2> Types;
  LLVM_DEBUG(dbgs() << "Building type info -> vtable map\n");

  auto *LTOPostLinkMD =
      cast_or_null<ConstantAsMetadata>(M.getModuleFlag("LTOPostLink"));
  bool LTOPostLink =
      LTOPostLinkMD &&
      (cast<ConstantInt>(LTOPostLinkMD->getValue())->getZExtValue() != 0);

  for (GlobalVariable &GV : M.globals()) {
    Types.clear();
    GV.getMetadata(LLVMContext::MD_type, Types);
    if (GV.isDeclaration() || Types.empty())
      continue;

    // Use the typeid metadata on the vtable to build a mapping from typeids to
    // the list of (GV, offset) pairs which are the possible vtables for that
    // typeid.
    for (MDNode *Type : Types) {
      Metadata *TypeID = Type->getOperand(1).get();

      uint64_t Offset =
          cast<ConstantInt>(
              cast<ConstantAsMetadata>(Type->getOperand(0))->getValue())
              ->getZExtValue();

      TypeIdMap[TypeID].insert(std::make_pair(&GV, Offset));
    }

    // If the type corresponding to the vtable is private to this translation
    // unit, we know that we can see all virtual functions which might use it,
    // so VFE is safe.
    if (auto GO = dyn_cast<GlobalObject>(&GV)) {
      GlobalObject::VCallVisibility TypeVis = GO->getVCallVisibility();
      if (TypeVis == GlobalObject::VCallVisibilityTranslationUnit ||
          (LTOPostLink &&
           TypeVis == GlobalObject::VCallVisibilityLinkageUnit)) {
        LLVM_DEBUG(dbgs() << GV.getName() << " is safe for VFE\n");
        VFESafeVTables.insert(&GV);
      }
    }
  }
}

void GlobalDCEPass::ScanVTableLoad(Function *Caller, Metadata *TypeId,
                                   uint64_t CallOffset) {
  for (auto &VTableInfo : TypeIdMap[TypeId]) {
    GlobalVariable *VTable = VTableInfo.first;
    uint64_t VTableOffset = VTableInfo.second;

    Constant *Ptr =
        getPointerAtOffset(VTable->getInitializer(), VTableOffset + CallOffset,
                           *Caller->getParent(), VTable);
    if (!Ptr) {
      LLVM_DEBUG(dbgs() << "can't find pointer in vtable!\n");
      VFESafeVTables.erase(VTable);
      continue;
    }

    auto Callee = dyn_cast<Function>(Ptr->stripPointerCastsSafe());
    if (!Callee) {
      LLVM_DEBUG(dbgs() << "vtable entry is not function pointer!\n");
      VFESafeVTables.erase(VTable);
      continue;
    }

    LLVM_DEBUG(dbgs() << "vfunc dep " << Caller->getName() << " -> "
                      << Callee->getName() << "\n");
    GVDependencies[Caller].insert(Callee);
  }
}

void GlobalDCEPass::ScanTypeCheckedLoadIntrinsics(Module &M) {
  LLVM_DEBUG(dbgs() << "Scanning type.checked.load intrinsics\n");
  Function *TypeCheckedLoadFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_checked_load));

  if (!TypeCheckedLoadFunc)
    return;

  for (auto U : TypeCheckedLoadFunc->users()) {
    auto CI = dyn_cast<CallInst>(U);
    if (!CI)
      continue;

    auto *Offset = dyn_cast<ConstantInt>(CI->getArgOperand(1));
    Value *TypeIdValue = CI->getArgOperand(2);
    auto *TypeId = cast<MetadataAsValue>(TypeIdValue)->getMetadata();

    if (Offset) {
      ScanVTableLoad(CI->getFunction(), TypeId, Offset->getZExtValue());
    } else {
      // type.checked.load with a non-constant offset, so assume every entry in
      // every matching vtable is used.
      for (auto &VTableInfo : TypeIdMap[TypeId]) {
        VFESafeVTables.erase(VTableInfo.first);
      }
    }
  }
}

void GlobalDCEPass::AddVirtualFunctionDependencies(Module &M) {
  if (!ClEnableVFE)
    return;

  // If the Virtual Function Elim module flag is present and set to zero, then
  // the vcall_visibility metadata was inserted for another optimization (WPD)
  // and we may not have type checked loads on all accesses to the vtable.
  // Don't attempt VFE in that case.
  auto *Val = mdconst::dyn_extract_or_null<ConstantInt>(
      M.getModuleFlag("Virtual Function Elim"));
  if (!Val || Val->getZExtValue() == 0)
    return;

  ScanVTables(M);

  if (VFESafeVTables.empty())
    return;

  ScanTypeCheckedLoadIntrinsics(M);

  LLVM_DEBUG(
    dbgs() << "VFE safe vtables:\n";
    for (auto *VTable : VFESafeVTables)
      dbgs() << "  " << VTable->getName() << "\n";
  );
}

PreservedAnalyses GlobalDCEPass::run(Module &M, ModuleAnalysisManager &MAM) {
  bool Changed = false;

  // The algorithm first computes the set L of global variables that are
  // trivially live.  Then it walks the initialization of these variables to
  // compute the globals used to initialize them, which effectively builds a
  // directed graph where nodes are global variables, and an edge from A to B
  // means B is used to initialize A.  Finally, it propagates the liveness
  // information through the graph starting from the nodes in L. Nodes note
  // marked as alive are discarded.

  // Remove empty functions from the global ctors list.
  Changed |= optimizeGlobalCtorsList(M, isEmptyFunction);

  // Collect the set of members for each comdat.
  for (Function &F : M)
    if (Comdat *C = F.getComdat())
      ComdatMembers.insert(std::make_pair(C, &F));
  for (GlobalVariable &GV : M.globals())
    if (Comdat *C = GV.getComdat())
      ComdatMembers.insert(std::make_pair(C, &GV));
  for (GlobalAlias &GA : M.aliases())
    if (Comdat *C = GA.getComdat())
      ComdatMembers.insert(std::make_pair(C, &GA));

  // Add dependencies between virtual call sites and the virtual functions they
  // might call, if we have that information.
  AddVirtualFunctionDependencies(M);

  // Loop over the module, adding globals which are obviously necessary.
  for (GlobalObject &GO : M.global_objects()) {
    GO.removeDeadConstantUsers();
    // Functions with external linkage are needed if they have a body.
    // Externally visible & appending globals are needed, if they have an
    // initializer.
    if (!GO.isDeclaration())
      if (!GO.isDiscardableIfUnused())
        MarkLive(GO);

    UpdateGVDependencies(GO);
  }

  // Compute direct dependencies of aliases.
  for (GlobalAlias &GA : M.aliases()) {
    GA.removeDeadConstantUsers();
    // Externally visible aliases are needed.
    if (!GA.isDiscardableIfUnused())
      MarkLive(GA);

    UpdateGVDependencies(GA);
  }

  // Compute direct dependencies of ifuncs.
  for (GlobalIFunc &GIF : M.ifuncs()) {
    GIF.removeDeadConstantUsers();
    // Externally visible ifuncs are needed.
    if (!GIF.isDiscardableIfUnused())
      MarkLive(GIF);

    UpdateGVDependencies(GIF);
  }

  // Propagate liveness from collected Global Values through the computed
  // dependencies.
  SmallVector<GlobalValue *, 8> NewLiveGVs{AliveGlobals.begin(),
                                           AliveGlobals.end()};
  while (!NewLiveGVs.empty()) {
    GlobalValue *LGV = NewLiveGVs.pop_back_val();
    for (auto *GVD : GVDependencies[LGV])
      MarkLive(*GVD, &NewLiveGVs);
  }

  // Now that all globals which are needed are in the AliveGlobals set, we loop
  // through the program, deleting those which are not alive.
  //

  // The first pass is to drop initializers of global variables which are dead.
  std::vector<GlobalVariable *> DeadGlobalVars; // Keep track of dead globals
  for (GlobalVariable &GV : M.globals())
    if (!AliveGlobals.count(&GV)) {
      DeadGlobalVars.push_back(&GV);         // Keep track of dead globals
      if (GV.hasInitializer()) {
        Constant *Init = GV.getInitializer();
        GV.setInitializer(nullptr);
        if (isSafeToDestroyConstant(Init))
          Init->destroyConstant();
      }
    }

  // The second pass drops the bodies of functions which are dead...
  std::vector<Function *> DeadFunctions;
  for (Function &F : M)
    if (!AliveGlobals.count(&F)) {
      DeadFunctions.push_back(&F);         // Keep track of dead globals
      if (!F.isDeclaration())
        F.deleteBody();
    }

  // The third pass drops targets of aliases which are dead...
  std::vector<GlobalAlias*> DeadAliases;
  for (GlobalAlias &GA : M.aliases())
    if (!AliveGlobals.count(&GA)) {
      DeadAliases.push_back(&GA);
      GA.setAliasee(nullptr);
    }

  // The fourth pass drops targets of ifuncs which are dead...
  std::vector<GlobalIFunc*> DeadIFuncs;
  for (GlobalIFunc &GIF : M.ifuncs())
    if (!AliveGlobals.count(&GIF)) {
      DeadIFuncs.push_back(&GIF);
      GIF.setResolver(nullptr);
    }

  //Drop instructions that were used only by GlobalVariables that we will be deleting
  llvm::DenseSet<Instruction*> insertedInstruction;
  std::vector<Instruction*> toProcessInstruction;
  llvm::DenseSet<ConstantExpr*> insertedConstExpr;
  std::vector<ConstantExpr*> toProcessConstExpr;
  auto InsertIfUnseen = [&](Value* V) {
    assert(V && "Null values shold not be possible");
    if (Instruction* I = dyn_cast<Instruction>(V)) {
      if (insertedInstruction.insert(I).second)
        toProcessInstruction.push_back(I);
    }
    else if (ConstantExpr* CE = dyn_cast<ConstantExpr>(V)) {
      if (insertedConstExpr.insert(CE).second)
        toProcessConstExpr.push_back(CE);
    }
    // Only Instructions and ConstantExprs are managed
  };

  for (GlobalVariable*GV : DeadGlobalVars)
    for (User *User : GV->users())
      if (isa<Instruction>(User) || isa<ConstantExpr>(User))
        InsertIfUnseen(User);

  for (Function* F : DeadFunctions)
    for (User *User : F->users())
      if (isa<Instruction>(User) || isa<ConstantExpr>(User))
        InsertIfUnseen(User);

  for (GlobalAlias *GA : DeadAliases)
    for (User *User : GA->users())
      if (isa<Instruction>(User) || isa<ConstantExpr>(User))
        InsertIfUnseen(User);

  for (GlobalIFunc *GIF : DeadIFuncs)
    for (User *User : GIF->users())
      if (isa<Instruction>(User) || isa<ConstantExpr>(User))
        InsertIfUnseen(User);

  //Collect Instructions or CEs that are to be deleted
  for (unsigned int i=0; i<toProcessConstExpr.size(); i++) {
    ConstantExpr* CE = toProcessConstExpr[i];
    for (User *User : CE->users())
      InsertIfUnseen(User);
  }

  //Collect more Instructions that are to be deleted
  for (unsigned int i=0; i<toProcessInstruction.size(); i++) {
    Instruction* I = toProcessInstruction[i];
    for (User *User : I->users())
      InsertIfUnseen(User);
  }

  //Erase Instructions (in two steps)
  for (Instruction* I : toProcessInstruction)
    I->replaceAllUsesWith(llvm::UndefValue::get(I->getType()));

  for (Instruction* I : toProcessInstruction)
    I->eraseFromParent();

  // Now that all interferences have been dropped, delete the actual objects
  // themselves.
  auto EraseUnusedGlobalValue = [&](GlobalValue *GV) {
    GV->removeDeadConstantUsers();
    GV->eraseFromParent();
    Changed = true;
  };

  NumFunctions += DeadFunctions.size();
  for (Function *F : DeadFunctions) {
    if (!F->use_empty()) {
      // Virtual functions might still be referenced by one or more vtables,
      // but if we've proven them to be unused then it's safe to replace the
      // virtual function pointers with null, allowing us to remove the
      // function itself.
      ++NumVFuncs;

      // Detect vfuncs that are referenced as "relative pointers" which are used
      // in Swift vtables, i.e. entries in the form of:
      //
      //   i32 trunc (i64 sub (i64 ptrtoint @f, i64 ptrtoint ...)) to i32)
      //
      // In this case, replace the whole "sub" expression with constant 0 to
      // avoid leaving a weird sub(0, symbol) expression behind.
      replaceRelativePointerUsersWithZero(F);

      F->replaceNonMetadataUsesWith(ConstantPointerNull::get(F->getType()));
    }
    EraseUnusedGlobalValue(F);
  }

  NumVariables += DeadGlobalVars.size();
  for (GlobalVariable *GV : DeadGlobalVars)
    EraseUnusedGlobalValue(GV);

  NumAliases += DeadAliases.size();
  for (GlobalAlias *GA : DeadAliases)
    EraseUnusedGlobalValue(GA);

  NumIFuncs += DeadIFuncs.size();
  for (GlobalIFunc *GIF : DeadIFuncs)
    EraseUnusedGlobalValue(GIF);

  // Make sure that all memory is released
  AliveGlobals.clear();
  ConstantDependenciesCache.clear();
  InstructionDependenciesCache.clear();
  GVDependencies.clear();
  ComdatMembers.clear();
  TypeIdMap.clear();
  VFESafeVTables.clear();

  if (Changed)
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
