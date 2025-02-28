//===-- StructMemFuncLowering.cpp - Cheerp helper -------------------------===//
//
//                     Cheerp: The C++ compiler for the Web
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Copyright 2014-2022 Leaning Technologies
//
//===----------------------------------------------------------------------===//

#include "llvm/InitializePasses.h"
#include "llvm/Cheerp/CommandLine.h"
#include "llvm/Cheerp/StructMemFuncLowering.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

const unsigned INLINE_WRITE_LOOP_MAX = 128;

void StructMemFuncLowering::createMemFunc(IRBuilder<>* IRB, Value* baseDst, Value* baseSrc, Type* containingType, size_t size,
						SmallVector<Value*, 8>& indexes)
{
	Value* src = IRB->CreateGEP(containingType, baseSrc, indexes);
	Value* dst = IRB->CreateGEP(containingType, baseDst, indexes);
	assert(!cast<GetElementPtrInst>(src)->getResultElementType()->isArrayTy());
	// Create a type safe memcpy
	IRB->CreateMemCpy(dst, MaybeAlign(), src, MaybeAlign(), size, false, NULL, NULL, NULL, NULL, llvm::IRBuilderBase::CheerpTypeInfo(cast<GetElementPtrInst>(src)->getResultElementType()));
}

void StructMemFuncLowering::recursiveCopy(IRBuilder<>* IRB, Value* baseDst, Value* baseSrc, Type* curType, Type* containingType,
						Type* indexType, uint32_t baseAlign, SmallVector<Value*, 8>& indexes)
{
	// For aggregates we push a new index and overwrite it for each element
	if(StructType* ST=dyn_cast<StructType>(curType))
	{
		assert(baseDst->getType() == containingType->getPointerTo());
		if (ST->hasByteLayout())
			return createMemFunc(IRB, baseDst, baseSrc, containingType, DL->getTypeAllocSize(curType), indexes);
		indexes.push_back(NULL);
		const StructLayout* SL = DL->getStructLayout(ST);
		for(uint32_t i=0;i<ST->getNumElements();i++)
		{
			indexes.back() = ConstantInt::get(indexType, i);
			uint32_t elemOffset = SL->getElementOffset(i);
			uint32_t elemAlign = baseAlign;
			while(elemOffset % elemAlign != 0)
				elemAlign /= 2;
			recursiveCopy(IRB, baseDst, baseSrc, ST->getElementType(i), containingType, indexType, elemAlign, indexes);
		}
		indexes.pop_back();
	}
	else if(ArrayType* AT=dyn_cast<ArrayType>(curType))
	{
		Type* elementType = AT->getElementType();
		indexes.push_back(NULL);
		uint32_t elemSize = DL->getTypeAllocSize(elementType);
		uint32_t elemAlign = baseAlign;
		while(elemSize % elemAlign != 0)
			elemAlign /= 2;
		if(AT->getNumElements() > 6)
		{
			// Create a loop instead of unrolling
			llvm::BasicBlock* prevBlock=IRB->GetInsertBlock();
			BasicBlock* arrayLoop=BasicBlock::Create(IRB->getContext(), "arrayloop", prevBlock->getParent());
			BasicBlock* afterLoop=BasicBlock::Create(IRB->getContext(), "afterloop", prevBlock->getParent());
			IRB->CreateBr(arrayLoop);
			IRB->SetInsertPoint(arrayLoop);
			llvm::PHINode* index=IRB->CreatePHI(indexType, 2);
			index->addIncoming(ConstantInt::get(indexType, 0), prevBlock);
			indexes.back() = index;
			recursiveCopy(IRB, baseDst, baseSrc, elementType, containingType, indexType, elemAlign, indexes);
			Value* incrementedIndex = IRB->CreateAdd(index, ConstantInt::get(indexType, 1));
			index->addIncoming(incrementedIndex, IRB->GetInsertBlock());
			Value* finishedLooping=IRB->CreateICmp(CmpInst::ICMP_EQ, ConstantInt::get(indexType, AT->getNumElements()), incrementedIndex);
			IRB->CreateCondBr(finishedLooping, afterLoop, arrayLoop);
			IRB->SetInsertPoint(afterLoop);
		}
		else
		{
			for(uint32_t i=0;i<AT->getNumElements();i++)
			{
				indexes.back() = ConstantInt::get(indexType, i);
				recursiveCopy(IRB, baseDst, baseSrc, elementType, containingType, indexType, elemAlign, indexes);
			}
		}
		indexes.pop_back();
	}
	else
	{
		Value* elementSrc = baseSrc;
		Value* elementDst = baseDst;
		Type* loadType = containingType;
		if(indexes.size() != 1 || !isa<ConstantInt>(indexes[0]) || cast<ConstantInt>(indexes[0])->getZExtValue()!=0)
		{
			assert(baseSrc->getType() == containingType->getPointerTo());
			assert(baseSrc->getType() == baseDst->getType());
			elementSrc = IRB->CreateGEP(containingType, baseSrc, indexes);
			elementDst = IRB->CreateGEP(containingType, baseDst, indexes);
			loadType = cast<GEPOperator>(elementDst)->getResultElementType();
		}
		assert(loadType->getPointerTo() == elementSrc->getType());
		Value* element = IRB->CreateAlignedLoad(loadType, elementSrc, MaybeAlign(baseAlign));
		IRB->CreateAlignedStore(element, elementDst, MaybeAlign(baseAlign));
	}
}

void StructMemFuncLowering::recursiveReset(IRBuilder<>* IRB, Value* baseDst, Value* resetVal, Type* curType, Type* containingType,
						Type* indexType, uint32_t baseAlign, SmallVector<Value*, 8>& indexes)
{
	// For aggregates we push a new index and overwrite it for each element
	if(StructType* ST=dyn_cast<StructType>(curType))
	{
		indexes.push_back(NULL);
		const StructLayout* SL = DL->getStructLayout(ST);
		for(uint32_t i=0;i<ST->getNumElements();i++)
		{
			indexes.back() = ConstantInt::get(indexType, i);
			uint32_t elemOffset = SL->getElementOffset(i);
			uint32_t elemAlign = baseAlign;
			while(elemOffset % elemAlign != 0)
				elemAlign /= 2;
			recursiveReset(IRB, baseDst, resetVal, ST->getElementType(i), containingType, indexType, elemAlign, indexes);
		}
		indexes.pop_back();
	}
	else if(ArrayType* AT=dyn_cast<ArrayType>(curType))
	{
		Type* elementType = AT->getElementType();
		indexes.push_back(NULL);
		uint32_t elemSize = DL->getTypeAllocSize(elementType);
		uint32_t elemAlign = baseAlign;
		while(elemSize % elemAlign != 0)
			elemAlign /= 2;
		if(AT->getNumElements() > 6)
		{
			// Create a loop instead of unrolling
			llvm::BasicBlock* prevBlock=IRB->GetInsertBlock();
			BasicBlock* arrayLoop=BasicBlock::Create(IRB->getContext(), "arrayloop", prevBlock->getParent());
			BasicBlock* afterLoop=BasicBlock::Create(IRB->getContext(), "afterloop", prevBlock->getParent());
			IRB->CreateBr(arrayLoop);
			IRB->SetInsertPoint(arrayLoop);
			llvm::PHINode* index=IRB->CreatePHI(indexType, 2);
			index->addIncoming(ConstantInt::get(indexType, 0), prevBlock);
			indexes.back() = index;
			recursiveReset(IRB, baseDst, resetVal, elementType, containingType, indexType, elemAlign, indexes);
			Value* incrementedIndex = IRB->CreateAdd(index, ConstantInt::get(indexType, 1));
			index->addIncoming(incrementedIndex, IRB->GetInsertBlock());
			Value* finishedLooping=IRB->CreateICmp(CmpInst::ICMP_EQ, ConstantInt::get(indexType, AT->getNumElements()), incrementedIndex);
			IRB->CreateCondBr(finishedLooping, afterLoop, arrayLoop);
			IRB->SetInsertPoint(afterLoop);
		}
		else
		{
			for(uint32_t i=0;i<AT->getNumElements();i++)
			{
				indexes.back() = ConstantInt::get(indexType, i);
				recursiveReset(IRB, baseDst, resetVal, elementType, containingType, indexType, elemAlign, indexes);
			}
		}
		indexes.pop_back();
	}
	else if(IntegerType* IT=dyn_cast<IntegerType>(curType))
	{
		int bitWidth = IT->getBitWidth();
		// We need to expand the resetVal 8-bit value to the right size
		Value* expandedResetVal = IRB->CreateZExtOrTrunc(resetVal,IT);
		Value* computedResetVal = expandedResetVal;
		for(int i=8;i<bitWidth;i+=8)
		{
			computedResetVal=IRB->CreateShl(computedResetVal, 8);
			computedResetVal=IRB->CreateOr(computedResetVal, expandedResetVal);
		}
		assert(containingType->getPointerTo() == baseDst->getType());
		Value* elementDst = IRB->CreateGEP(containingType, baseDst, indexes);
		IRB->CreateAlignedStore(computedResetVal, elementDst, MaybeAlign(baseAlign));
	}
	else if(curType->isFloatTy() || curType->isDoubleTy())
	{
		// Only constant values are supported
		// TODO: Stop non constant in the frontend
		uint8_t constResetVal = cast<ConstantInt>( resetVal )->getZExtValue();
		int bytesNum = curType->isFloatTy() ? 4 : 8;
		uint64_t floatConstant = 0;
		for(int i=0;i<bytesNum;i++)
		{
			floatConstant <<= 8;
			floatConstant |= constResetVal;
		}
		Value* floatResetVal = NULL;
		if(curType->isFloatTy())
			floatResetVal = ConstantFP::get(curType->getContext(), APFloat(APFloat::IEEEsingle(), floatConstant));
		else
			floatResetVal = ConstantFP::get(curType->getContext(), APFloat(APFloat::IEEEdouble(), floatConstant));
		assert(containingType->getPointerTo() == baseDst->getType());
		Value* elementDst = IRB->CreateGEP(containingType, baseDst, indexes);
		IRB->CreateAlignedStore(floatResetVal, elementDst, MaybeAlign(baseAlign));
	}
	else if(PointerType* PT=dyn_cast<PointerType>(curType))
	{
		// Only constant NULL is supported
		// TODO: Stop non constant in the frontend
		assert(cast<ConstantInt>( resetVal )->getZExtValue() == 0);
		assert(containingType->getPointerTo() == baseDst->getType());
		Value* elementDst = IRB->CreateGEP(containingType, baseDst, indexes);
		IRB->CreateAlignedStore(ConstantPointerNull::get(PT), elementDst, MaybeAlign(baseAlign));
	}
	else
	{
		assert(false && "Unexpected type while unrolling memset");
	}
}

// Create a generalized loop, the index is either incremented until elementsCount is reached
// or decremented from elementsCount-1 to 0. IRB already inserts inside currentBlock.
void StructMemFuncLowering::createGenericLoop(IRBuilder<>* IRB, BasicBlock* previousBlock, BasicBlock* endBlock, BasicBlock* currentBlock,
						Type* pointedType, Value* dst, Value* src, Value* elementsCount, MODE mode, uint32_t baseAlign, const bool isForward)
{
	Type* int32Type = IntegerType::get(previousBlock->getContext(), 32);
	const bool needsLoop = !isa<ConstantInt>(elementsCount) || cast<ConstantInt>(elementsCount)->getZExtValue() != 1;

	Value* endPtr = NULL;

	//These are loop-invariants, so are moved to the end of the previous block
	{
		IRB->SetInsertPoint(previousBlock->getTerminator());
		if (needsLoop)
			endPtr = IRB->CreateInBoundsGEP(pointedType, dst, isForward ? elementsCount : ConstantInt::get(int32Type, 0));
		if (!isForward)
		{
			assert(mode == MEMMOVE);
			//Move src and dst past the end of the range (we copy backwards)
			src = IRB->CreateInBoundsGEP(pointedType, src, elementsCount);
			dst = IRB->CreateInBoundsGEP(pointedType, dst, elementsCount);
		}
		IRB->SetInsertPoint(currentBlock);
	}

	PHINode* srcPHI = NULL;
	PHINode* dstPHI = NULL;
	Value* srcVal = src;
	Value* dstVal = dst;

	if(needsLoop)
	{
		if(mode != MEMSET)
		{
			srcPHI = IRB->CreatePHI(pointedType->getPointerTo(), 2);
			srcPHI->addIncoming(src, previousBlock);
			srcVal = srcPHI;
		}
		dstPHI = IRB->CreatePHI(pointedType->getPointerTo(), 2);
		dstPHI->addIncoming(dst, previousBlock);
		dstVal = dstPHI;
	}

	// Immediately decrement by one, so that we are accessing a valid elements
	if (!isForward)
	{
		srcVal = IRB->CreateInBoundsGEP(pointedType, srcVal, ConstantInt::get(int32Type, -1));
		dstVal = IRB->CreateInBoundsGEP(pointedType, dstVal, ConstantInt::get(int32Type, -1));
	}

	// Now, recursively descend into the object to copy all the values
	SmallVector<Value*, 8> indexes;
	indexes.push_back(ConstantInt::get(int32Type, 0));

	if (mode == MEMSET)
		recursiveReset(IRB, dstVal, srcVal, pointedType, pointedType, int32Type, baseAlign, indexes);
	else
		recursiveCopy(IRB, dstVal, srcVal, pointedType, pointedType, int32Type, baseAlign, indexes);

	if(needsLoop)
	{
		// Increment the pointer(s) now
		if (isForward)
		{
			if (mode != MEMSET)
				srcVal = IRB->CreateInBoundsGEP(pointedType, srcVal, ConstantInt::get(int32Type, 1));
			dstVal = IRB->CreateInBoundsGEP(pointedType, dstVal, ConstantInt::get(int32Type, 1));
		}

		// Close the loop for dst (and src)
		if(mode != MEMSET)
			srcPHI->addIncoming(srcVal, IRB->GetInsertBlock());
		dstPHI->addIncoming(dstVal, IRB->GetInsertBlock());

		// Check if we have finished, if not loop again
		Value* finishedLooping=IRB->CreateICmp(CmpInst::ICMP_EQ, endPtr, dstVal);
		IRB->CreateCondBr(finishedLooping, endBlock, currentBlock);
	}
	else
		IRB->CreateBr(endBlock);
}

// Create a forward loop, the index is incremented until elementsCount is reached. IRB already inserts inside currentBlock.
void StructMemFuncLowering::createForwardLoop(IRBuilder<>* IRB, BasicBlock* previousBlock, BasicBlock* endBlock, BasicBlock* currentBlock,
						Type* pointedType, Value* dst, Value* src, Value* elementsCount, MODE mode, uint32_t baseAlign)
{
	createGenericLoop(IRB, previousBlock, endBlock, currentBlock, pointedType, dst, src, elementsCount, mode, baseAlign, /*isForward*/true);
}

// Create a backward loop, the index is increment from elementsCount-1 to 0. IRB already inserts inside currentBlock.
void StructMemFuncLowering::createBackwardLoop(IRBuilder<>* IRB, BasicBlock* previousBlock, BasicBlock* endBlock, BasicBlock* currentBlock,
						Type* pointedType, Value* dst, Value* src, Value* elementsCount, uint32_t baseAlign)
{
	createGenericLoop(IRB, previousBlock, endBlock, currentBlock, pointedType, dst, src, elementsCount, MODE::MEMMOVE, baseAlign, /*isForward*/false);
}

bool StructMemFuncLowering::createLoops(llvm::BasicBlock& BB, llvm::BasicBlock* endLoop, llvm::Type* int32Type, llvm::Value* src, llvm::Value* dst, llvm::Value* size, llvm::Type* pointedType, MODE mode, uint32_t baseAlign)
{
	assert(dst->getType() == src->getType() || mode==MEMSET);
	uint32_t byteSize = DL->getTypeAllocSize(pointedType);
	IRBuilder<> IRB(&BB);
	Value* fixedSize = IRB.CreateZExtOrTrunc(size, int32Type);
	Value* elementsCount=IRB.CreateUDiv(fixedSize, ConstantInt::get(int32Type, byteSize));
	Value* countIsZero=IRB.CreateICmp(CmpInst::ICMP_EQ, elementsCount, ConstantInt::get(int32Type, 0));
	// Handle the cases when countIsZero is a constant. This pass is the very last one in the LTO phase and inefficient code is left otherwise.
	ConstantInt* constantCondition = dyn_cast<ConstantInt>(countIsZero);
	if(constantCondition && !constantCondition->isZeroValue())
	{
		// Nothing to do
		return false;
	}
	BasicBlock* memfuncBody=BasicBlock::Create(BB.getContext(), "memfunc.body", BB.getParent());
	if(constantCondition && constantCondition->isZeroValue())
		IRB.CreateBr(memfuncBody);
	else
		IRB.CreateCondBr(countIsZero, endLoop, memfuncBody);
	IRB.SetInsertPoint(memfuncBody);
	if (mode == MEMMOVE)
	{
		// For memmove we need to check the relative ordering of src and dst and select a direction accordingly
		Value* srcAfterDst = IRB.CreateICmp(CmpInst::ICMP_UGE, src, dst);
		// Create two basic blocks, one is for the forward case, the other for the backward case
		BasicBlock* memmoveForward=BasicBlock::Create(BB.getContext(), "memmove.forward", BB.getParent());
		BasicBlock* memmoveBackward=BasicBlock::Create(BB.getContext(), "memmove.backward", BB.getParent());
		IRB.CreateCondBr(srcAfterDst, memmoveForward, memmoveBackward);
		// Do the forward side
		IRB.SetInsertPoint(memmoveForward);
		createForwardLoop(&IRB, memfuncBody, endLoop, memmoveForward, pointedType, dst, src, elementsCount, mode, baseAlign);
		// Do the backward side
		IRB.SetInsertPoint(memmoveBackward);
		createBackwardLoop(&IRB, memfuncBody, endLoop, memmoveBackward, pointedType, dst, src, elementsCount, baseAlign);
	}
	else //if(mode == MEMCPY || mode == MEMSET)
		createForwardLoop(&IRB, &BB, endLoop, memfuncBody, pointedType, dst, src, elementsCount, mode, baseAlign);
	return true;
}

bool StructMemFuncLowering::runOnBlock(BasicBlock& BB, bool asmjs)
{
	BasicBlock::iterator it=BB.begin();
	BasicBlock::iterator itE=BB.end();
	for(;it!=itE;++it)
	{
		CallInst* CI=dyn_cast<CallInst>(it);
		if(!CI || !CI->getCalledFunction())
			continue;
		Function* F=CI->getCalledFunction();
		MODE mode = NONE;
		if(F->getIntrinsicID()==Intrinsic::memcpy)
			mode = MEMCPY;
		else if(F->getIntrinsicID()==Intrinsic::memmove)
			mode = MEMMOVE;
		else if(F->getIntrinsicID()==Intrinsic::memset)
			mode = MEMSET;
		if(mode==NONE)
			continue;
		Type* pointedType = CI->getParamElementType(0);
		assert(pointedType);
		uint32_t alignInt = 0;
		//We want to decompose everything which is not a byte layout structure. memset is always decomposed.
		if(mode != MEMSET)
		{
			bool isByteLayout = isa<StructType>(pointedType) && cast<StructType>(pointedType)->hasByteLayout();
			if(isByteLayout)
				continue;
			MemTransferInst* MTI = cast<MemTransferInst>(CI);
			alignInt = std::min(MTI->getSourceAlignment(), MTI->getDestAlignment());
		}
		else
		{
			alignInt = cast<MemSetInst>(CI)->getDestAlignment();
		}

		//We have a typed mem func on a struct
		//Decompose it in a loop
		Value* dst=CI->getOperand(0);
		//In MEMSET mode src is the value to be written
		Value* src=CI->getOperand(1);
		Value* size=CI->getOperand(2);
		Type* int32Type = IntegerType::get(BB.getContext(), 32);
		assert(alignInt != 0);
		// Do not inline memory intrinsics with a large or non-constant size
		// argument, when in linear memory mode.
		// Also, if the memcpy is nicely aligned, use 32 bit loads and stores
		if (asmjs) {
			ConstantInt *sizeConst = dyn_cast<ConstantInt>(size);
			if (!sizeConst || sizeConst->getZExtValue() > INLINE_WRITE_LOOP_MAX)
				continue;
			bool useUnaligned = LinearOutput == Wasm && !WasmNoUnalignedMem && mode != MEMMOVE;
			uint32_t effectiveAlignInt = alignInt;
			if(useUnaligned)
			{
				// In wasm unaligned memory accesses are allowed
				// NOTE: This would not be safe in MEMMOVE mode, we only have
				//       guaranteed non-overlap when the alignment is larger than the element size
				effectiveAlignInt = 0;
			}
			uint32_t sizeInt = sizeConst->getZExtValue();
			uint32_t elemSize = 1;
			if (LinearOutput == Wasm && effectiveAlignInt % 8 == 0 && sizeInt >= 8) {
				// i64 can only be used in wasm mode
				Type* int64Type = IntegerType::get(BB.getContext(), 64);
				pointedType = int64Type;
				elemSize = 8;
			} else if (effectiveAlignInt % 4 == 0 && sizeInt >= 4) {
				pointedType = int32Type;
				elemSize = 4;
			} else if (effectiveAlignInt % 2 == 0 && sizeInt >= 2) {
				pointedType = IntegerType::get(BB.getContext(), 16);
				elemSize = 2;
			}
			if (elemSize > 1) {
				// Unroll small loops
				// NOTE: Moving more than 1 element per iteration is only safe if there is no overlapping
				if(mode != MEMMOVE && (sizeInt / elemSize <= 3)) {
					// Unrolling works by forging a virtual [N x pointedType] array for the memory
					pointedType = ArrayType::get(pointedType, sizeInt / elemSize);
					// The elem size is the whole array size, there may be an extra tail
					elemSize = sizeInt - (sizeInt % elemSize);
				}
				IRBuilder<> IRB(CI);
				dst = IRB.CreateBitCast(dst, pointedType->getPointerTo());
				// In MEMSET mode src is the i8 value to write
				if(mode != MEMSET)
					src = IRB.CreateBitCast(src, pointedType->getPointerTo());
				// We have found a good alignment above, check if we need to split the intrinsic to deal with an unaligned tail
				if(uint32_t tailSize = sizeInt % elemSize) {
					IRBuilder<> IRB(CI->getNextNode());
					uint32_t skipCount = sizeInt / elemSize;
					llvm::Value* tailDst = IRB.CreateGEP(pointedType, dst, ConstantInt::get(int32Type, skipCount));
					llvm::Value* tailSrc = src;
					if(mode != MEMSET)
						tailSrc = IRB.CreateGEP(pointedType, src, ConstantInt::get(int32Type, skipCount));
					uint32_t newAlign = alignInt;
					// The tail operation starts at a multiple of elemSize
					while(elemSize % newAlign != 0)
						newAlign /= 2;
					if(mode == MEMCPY)
						IRB.CreateMemCpy(tailDst, MaybeAlign(newAlign), tailSrc, MaybeAlign(newAlign), tailSize);
					else if(mode == MEMMOVE)
						IRB.CreateMemMove(tailDst, MaybeAlign(newAlign), tailSrc, MaybeAlign(newAlign), tailSize);
					else //if(mode == MEMSET)
						IRB.CreateMemSet(tailDst, tailSrc, tailSize, MaybeAlign(newAlign));
					size = ConstantInt::get(int32Type, sizeInt - tailSize);
				}
			}
		}
		//First of all split the original block
		BasicBlock* endLoop = BB.splitBasicBlock(it);
		//Add the lower part of the block to be anaysized later on
		basicBlocks.push_back(endLoop);
		//Now BB is linked to endblock by an unconditional jump
		//Instead we need to check if we need to enter the loop
		Instruction* oldBranch = BB.getTerminator();
		if(createLoops(BB, endLoop, int32Type, src, dst, size, pointedType, mode, alignInt)) {
			//Delete the old branch
			oldBranch->eraseFromParent();
		}
		CI->eraseFromParent();
		return true;
	}
	return false;
}

bool StructMemFuncLowering::runOnFunction(Function& F)
{
	DL = &F.getParent()->getDataLayout();
	assert(DL);
	for(BasicBlock& BB: F)
		basicBlocks.push_back(&BB);
	while(!basicBlocks.empty())
	{
		BasicBlock* BB = basicBlocks.pop_back_val();
		runOnBlock(*BB, F.getSection() == StringRef("asmjs"));
	}
	return true;
}

PreservedAnalyses StructMemFuncLoweringPass::run(Function& F, FunctionAnalysisManager& FAM)
{
	StructMemFuncLowering inner;
	if (inner.runOnFunction(F))
		return PreservedAnalyses::none();
	return PreservedAnalyses::all();
}
