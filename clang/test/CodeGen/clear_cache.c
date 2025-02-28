// NOTE: Assertions have been autogenerated by utils/update_cc_test_checks.py
// RUN: %clang_cc1 -triple x86_64-linux-gnu -emit-llvm %s -o - | FileCheck %s

char buffer[32] = "This is a largely unused buffer";

// __builtin___clear_cache always maps to @llvm.clear_cache, but what
// each back-end produces is different, and this is tested in LLVM

// CHECK-LABEL: @main(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RETVAL:%.*]] = alloca i32, align 4
// CHECK-NEXT:    store i32 0, i32* [[RETVAL]], align 4
// CHECK-NEXT:    call void @llvm.clear_cache(i8* getelementptr inbounds ([32 x i8], [32 x i8]* @buffer, i64 0, i64 0), i8* getelementptr inbounds ([32 x i8], [32 x i8]* @buffer, i64 0, i64 32))
// CHECK-NEXT:    ret i32 0
//
int main(void) {
  __builtin___clear_cache(buffer, buffer+32);
  return 0;
}
