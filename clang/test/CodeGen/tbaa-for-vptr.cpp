// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -o - -fsanitize=thread %s | FileCheck %s --check-prefixes=CHECK,OLD-PATH
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -o - -O1 -fno-experimental-new-pass-manager %s | FileCheck %s --check-prefixes=CHECK,OLD-PATH
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -o - -O1 -fno-experimental-new-pass-manager -relaxed-aliasing -fsanitize=thread %s | FileCheck %s --check-prefixes=CHECK,OLD-PATH
//
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -new-struct-path-tbaa -o - -fsanitize=thread %s | FileCheck %s --check-prefixes=CHECK,NEW-PATH
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -new-struct-path-tbaa -o - -O1 -fno-experimental-new-pass-manager %s | FileCheck %s --check-prefixes=CHECK,NEW-PATH
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -new-struct-path-tbaa -o - -O1 -fno-experimental-new-pass-manager -relaxed-aliasing -fsanitize=thread %s | FileCheck %s --check-prefixes=CHECK,NEW-PATH
//
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -o - %s | FileCheck %s --check-prefix=NOTBAA
// RUN: %clang_cc1 -triple %itanium_abi_triple -emit-llvm -o - -O2 -fno-experimental-new-pass-manager -relaxed-aliasing %s | FileCheck %s --check-prefix=NOTBAA
//
// Check that we generate TBAA for vtable pointer loads and stores.
// When -fsanitize=thread is used TBAA should be generated at all opt levels
// even if -relaxed-aliasing is present.
struct A {
  virtual int foo() const ;
  virtual ~A();
};

void CreateA() {
  new A;
}

void CallFoo(A *a, int (A::*fp)() const) {
  a->foo();
  (a->*fp)();
}

// CHECK-LABEL: @_Z7CallFoo
// CHECK: %{{.*}} = load i32 ({{.*}})**, {{.*}} !tbaa ![[NUM:[0-9]+]]
// CHECK: br i1
// CHECK: load {{.*}}*, {{.*}}, !tbaa ![[NUM]]
//
// CHECK-LABEL: @_ZN1AC2Ev
// CHECK: store i32 (...)** {{.*}}, !tbaa ![[NUM]]
//
// OLD-PATH: [[NUM]] = !{[[TYPE:!.*]], [[TYPE]], i64 0}
// OLD-PATH: [[TYPE]] = !{!"vtable pointer", !{{.*}}
// NEW-PATH: [[NUM]] = !{[[TYPE:!.*]], [[TYPE]], i64 0, i64 [[POINTER_SIZE:.*]]}
// NEW-PATH: [[TYPE]] = !{!{{.*}}, i64 [[POINTER_SIZE]], !"vtable pointer"}
// NOTBAA-NOT: = !{!"Simple C++ TBAA"}
