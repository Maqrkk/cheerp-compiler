; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -mtriple=x86_64-unknown-linux -slp-vectorizer -S | FileCheck %s --check-prefixes=CHECK,SSE
; RUN: opt < %s -mtriple=x86_64-unknown-linux -mcpu=corei7-avx -slp-vectorizer -S | FileCheck %s --check-prefixes=CHECK,AVX
; RUN: opt < %s -mtriple=x86_64-unknown-linux -mcpu=core-avx2 -slp-vectorizer -S | FileCheck %s --check-prefixes=CHECK,AVX

@arr = local_unnamed_addr global [32 x i32] zeroinitializer, align 16

declare i32 @llvm.smax.i32(i32, i32)

define i32 @smax_v2i32(i32) {
; CHECK-LABEL: @smax_v2i32(
; CHECK-NEXT:    [[TMP2:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
; CHECK-NEXT:    [[TMP3:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
; CHECK-NEXT:    [[TMP4:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP2]], i32 [[TMP3]])
; CHECK-NEXT:    ret i32 [[TMP4]]
;
  %2 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
  %3 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
  %4 = call i32 @llvm.smax.i32(i32 %2, i32 %3)
  ret i32 %4
}

define i32 @smax_v4i32(i32) {
; SSE-LABEL: @smax_v4i32(
; SSE-NEXT:    [[TMP2:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
; SSE-NEXT:    [[TMP3:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
; SSE-NEXT:    [[TMP4:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 2), align 8
; SSE-NEXT:    [[TMP5:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 3), align 4
; SSE-NEXT:    [[TMP6:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP2]], i32 [[TMP3]])
; SSE-NEXT:    [[TMP7:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP6]], i32 [[TMP4]])
; SSE-NEXT:    [[TMP8:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP7]], i32 [[TMP5]])
; SSE-NEXT:    ret i32 [[TMP8]]
;
; AVX-LABEL: @smax_v4i32(
; AVX-NEXT:    [[TMP2:%.*]] = load <4 x i32>, <4 x i32>* bitcast (i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0) to <4 x i32>*), align 16
; AVX-NEXT:    [[TMP3:%.*]] = call i32 @llvm.vector.reduce.smax.v4i32(<4 x i32> [[TMP2]])
; AVX-NEXT:    ret i32 [[TMP3]]
;
  %2 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
  %3 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
  %4 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 2), align 8
  %5 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 3), align 4
  %6 = call i32 @llvm.smax.i32(i32 %2, i32 %3)
  %7 = call i32 @llvm.smax.i32(i32 %6, i32 %4)
  %8 = call i32 @llvm.smax.i32(i32 %7, i32 %5)
  ret i32 %8
}

define i32 @smax_v8i32(i32) {
; SSE-LABEL: @smax_v8i32(
; SSE-NEXT:    [[TMP2:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
; SSE-NEXT:    [[TMP3:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
; SSE-NEXT:    [[TMP4:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 2), align 8
; SSE-NEXT:    [[TMP5:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 3), align 4
; SSE-NEXT:    [[TMP6:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 4), align 16
; SSE-NEXT:    [[TMP7:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 5), align 4
; SSE-NEXT:    [[TMP8:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 6), align 8
; SSE-NEXT:    [[TMP9:%.*]] = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 7), align 4
; SSE-NEXT:    [[TMP10:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP2]], i32 [[TMP3]])
; SSE-NEXT:    [[TMP11:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP10]], i32 [[TMP4]])
; SSE-NEXT:    [[TMP12:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP11]], i32 [[TMP5]])
; SSE-NEXT:    [[TMP13:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP12]], i32 [[TMP6]])
; SSE-NEXT:    [[TMP14:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP13]], i32 [[TMP7]])
; SSE-NEXT:    [[TMP15:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP14]], i32 [[TMP8]])
; SSE-NEXT:    [[TMP16:%.*]] = call i32 @llvm.smax.i32(i32 [[TMP15]], i32 [[TMP9]])
; SSE-NEXT:    ret i32 [[TMP16]]
;
; AVX-LABEL: @smax_v8i32(
; AVX-NEXT:    [[TMP2:%.*]] = load <8 x i32>, <8 x i32>* bitcast (i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0) to <8 x i32>*), align 16
; AVX-NEXT:    [[TMP3:%.*]] = call i32 @llvm.vector.reduce.smax.v8i32(<8 x i32> [[TMP2]])
; AVX-NEXT:    ret i32 [[TMP3]]
;
  %2 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
  %3 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
  %4 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 2), align 8
  %5 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 3), align 4
  %6 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 4), align 16
  %7 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 5), align 4
  %8 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 6), align 8
  %9 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 7), align 4
  %10 = call i32 @llvm.smax.i32(i32 %2,  i32 %3)
  %11 = call i32 @llvm.smax.i32(i32 %10, i32 %4)
  %12 = call i32 @llvm.smax.i32(i32 %11, i32 %5)
  %13 = call i32 @llvm.smax.i32(i32 %12, i32 %6)
  %14 = call i32 @llvm.smax.i32(i32 %13, i32 %7)
  %15 = call i32 @llvm.smax.i32(i32 %14, i32 %8)
  %16 = call i32 @llvm.smax.i32(i32 %15, i32 %9)
  ret i32 %16
}

define i32 @smax_v16i32(i32) {
; CHECK-LABEL: @smax_v16i32(
; CHECK-NEXT:    [[TMP2:%.*]] = load <16 x i32>, <16 x i32>* bitcast (i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0) to <16 x i32>*), align 16
; CHECK-NEXT:    [[TMP3:%.*]] = call i32 @llvm.vector.reduce.smax.v16i32(<16 x i32> [[TMP2]])
; CHECK-NEXT:    ret i32 [[TMP3]]
;
  %2  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 0), align 16
  %3  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 1), align 4
  %4  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 2), align 8
  %5  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 3), align 4
  %6  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 4), align 16
  %7  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 5), align 4
  %8  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 6), align 8
  %9  = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 7), align 4
  %10 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 8), align 16
  %11 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 9), align 4
  %12 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 10), align 8
  %13 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 11), align 4
  %14 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 12), align 16
  %15 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 13), align 4
  %16 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 14), align 8
  %17 = load i32, i32* getelementptr inbounds ([32 x i32], [32 x i32]* @arr, i64 0, i64 15), align 4
  %18 = call i32 @llvm.smax.i32(i32 %2,  i32 %3)
  %19 = call i32 @llvm.smax.i32(i32 %18, i32 %4)
  %20 = call i32 @llvm.smax.i32(i32 %19, i32 %5)
  %21 = call i32 @llvm.smax.i32(i32 %20, i32 %6)
  %22 = call i32 @llvm.smax.i32(i32 %21, i32 %7)
  %23 = call i32 @llvm.smax.i32(i32 %22, i32 %8)
  %24 = call i32 @llvm.smax.i32(i32 %23, i32 %9)
  %25 = call i32 @llvm.smax.i32(i32 %24, i32 %10)
  %26 = call i32 @llvm.smax.i32(i32 %25, i32 %11)
  %27 = call i32 @llvm.smax.i32(i32 %26, i32 %12)
  %28 = call i32 @llvm.smax.i32(i32 %27, i32 %13)
  %29 = call i32 @llvm.smax.i32(i32 %28, i32 %14)
  %30 = call i32 @llvm.smax.i32(i32 %29, i32 %15)
  %31 = call i32 @llvm.smax.i32(i32 %30, i32 %16)
  %32 = call i32 @llvm.smax.i32(i32 %31, i32 %17)
  ret i32 %32
}
