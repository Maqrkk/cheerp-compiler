; RUN: opt < %s -S -passes="msan<track-origins=1>" 2>&1 | FileCheck %s --implicit-check-not "call void @llvm.mem" --implicit-check-not " load" --implicit-check-not " store"
; RUN: opt < %s -S -msan -msan-track-origins=1 | FileCheck %s --implicit-check-not "call void @llvm.mem" --implicit-check-not " load" --implicit-check-not " store"

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @FnByVal(i128* byval(i128) %p);
declare void @Fn(i128* %p);

define i128 @ByValArgument(i32, i128* byval(i128) %p) sanitize_memory {
; CHECK-LABEL: @ByValArgument(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 8 %[[#]], i8* align 8 inttoptr (i64 add (i64 ptrtoint ([100 x i64]* @__msan_param_tls to i64), i64 8) to i8*), i32 16, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %[[#]], i8* align 4 inttoptr (i64 add (i64 ptrtoint ([200 x i32]* @__msan_param_origin_tls to i64), i64 8) to i8*), i32 16, i1 false)
; CHECK:         [[X:%.*]] = load i128, i128* %p, align 8
; CHECK:         [[_MSLD:%.*]] = load i128, i128* %[[#]], align 8
; CHECK:         %[[#]] = load i32, i32* %[[#]], align 8
; CHECK:         store i128 [[_MSLD]], i128* bitcast ([100 x i64]* @__msan_retval_tls to i128*), align 8
; CHECK:         store i32 %[[#]], i32* @__msan_retval_origin_tls, align 4
; CHECK:         ret i128 [[X]]
;
entry:
  %x = load i128, i128* %p
  ret i128 %x
}

define i128 @ByValArgumentNoSanitize(i32, i128* byval(i128) %p) {
; CHECK-LABEL: @ByValArgumentNoSanitize(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memset.p0i8.i32(i8* align 8 %[[#]], i8 0, i32 16, i1 false)
; CHECK:         [[X:%.*]] = load i128, i128* %p, align 8
; CHECK:         store i128 0, i128* bitcast ([100 x i64]* @__msan_retval_tls to i128*), align 8
; CHECK:         store i32 0, i32* @__msan_retval_origin_tls, align 4
; CHECK:         ret i128 [[X]]
;
entry:
  %x = load i128, i128* %p
  ret i128 %x
}

define void @ByValForward(i32, i128* byval(i128) %p) sanitize_memory {
; CHECK-LABEL: @ByValForward(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 8 %[[#]], i8* align 8 inttoptr (i64 add (i64 ptrtoint ([100 x i64]* @__msan_param_tls to i64), i64 8) to i8*), i32 16, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %[[#]], i8* align 4 inttoptr (i64 add (i64 ptrtoint ([200 x i32]* @__msan_param_origin_tls to i64), i64 8) to i8*), i32 16, i1 false)
; CHECK:         store i64 0, i64* bitcast ([100 x i64]* @__msan_param_tls to i64*), align 8
; CHECK:         call void @Fn(i128* %p)
; CHECK:         ret void
;
entry:
  call void @Fn(i128* %p)
  ret void
}

define void @ByValForwardNoSanitize(i32, i128* byval(i128) %p) {
; CHECK-LABEL: @ByValForwardNoSanitize(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memset.p0i8.i32(i8* align 8 %[[#]], i8 0, i32 16, i1 false)
; CHECK:         store i64 0, i64* bitcast ([100 x i64]* @__msan_param_tls to i64*), align 8
; CHECK:         call void @Fn(i128* %p)
; CHECK:         ret void
;
entry:
  call void @Fn(i128* %p)
  ret void
}

define void @ByValForwardByVal(i32, i128* byval(i128) %p) sanitize_memory {
; CHECK-LABEL: @ByValForwardByVal(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 8 %[[#]], i8* align 8 inttoptr (i64 add (i64 ptrtoint ([100 x i64]* @__msan_param_tls to i64), i64 8) to i8*), i32 16, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %[[#]], i8* align 4 inttoptr (i64 add (i64 ptrtoint ([200 x i32]* @__msan_param_origin_tls to i64), i64 8) to i8*), i32 16, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* bitcast ([100 x i64]* @__msan_param_tls to i8*), i8* %[[#]], i32 16, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 bitcast ([200 x i32]* @__msan_param_origin_tls to i8*), i8* align 4 %[[#]], i32 16, i1 false)
; CHECK:         call void @FnByVal(i128* byval(i128) %p)
; CHECK:         ret void
;
entry:
  call void @FnByVal(i128* byval(i128) %p)
  ret void
}

define void @ByValForwardByValNoSanitize(i32, i128* byval(i128) %p) {
; CHECK-LABEL: @ByValForwardByValNoSanitize(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memset.p0i8.i32(i8* align 8 %[[#]], i8 0, i32 16, i1 false)
; CHECK:         call void @llvm.memset.p0i8.i32(i8* bitcast ([100 x i64]* @__msan_param_tls to i8*), i8 0, i32 16, i1 false)
; CHECK:         call void @FnByVal(i128* byval(i128) %p)
; CHECK:         ret void
;
entry:
  call void @FnByVal(i128* byval(i128) %p)
  ret void
}

declare void @FnByVal8(i8* byval(i8) %p);
declare void @Fn8(i8* %p);

define i8 @ByValArgument8(i32, i8* byval(i8) %p) sanitize_memory {
; CHECK-LABEL: @ByValArgument8(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 1 %[[#]], i8* align 1 inttoptr (i64 add (i64 ptrtoint ([100 x i64]* @__msan_param_tls to i64), i64 8) to i8*), i32 1, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %[[#]], i8* align 4 inttoptr (i64 add (i64 ptrtoint ([200 x i32]* @__msan_param_origin_tls to i64), i64 8) to i8*), i32 4, i1 false)
; CHECK:         [[X:%.*]] = load i8, i8* %p, align 1
; CHECK:         [[_MSLD:%.*]] = load i8, i8* %[[#]], align 1
; CHECK:         %[[#]] = load i32, i32* %[[#]], align 4
; CHECK:         store i8 [[_MSLD]], i8* bitcast ([100 x i64]* @__msan_retval_tls to i8*), align 8
; CHECK:         store i32 %[[#]], i32* @__msan_retval_origin_tls, align 4
; CHECK:         ret i8 [[X]]
;
entry:
  %x = load i8, i8* %p
  ret i8 %x
}

define i8 @ByValArgumentNoSanitize8(i32, i8* byval(i8) %p) {
; CHECK-LABEL: @ByValArgumentNoSanitize8(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memset.p0i8.i32(i8* align 1 %[[#]], i8 0, i32 1, i1 false)
; CHECK:         [[X:%.*]] = load i8, i8* %p, align 1
; CHECK:         store i8 0, i8* bitcast ([100 x i64]* @__msan_retval_tls to i8*), align 8
; CHECK:         store i32 0, i32* @__msan_retval_origin_tls, align 4
; CHECK:         ret i8 [[X]]
;
entry:
  %x = load i8, i8* %p
  ret i8 %x
}

define void @ByValForward8(i32, i8* byval(i8) %p) sanitize_memory {
; CHECK-LABEL: @ByValForward8(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 1 %[[#]], i8* align 1 inttoptr (i64 add (i64 ptrtoint ([100 x i64]* @__msan_param_tls to i64), i64 8) to i8*), i32 1, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %[[#]], i8* align 4 inttoptr (i64 add (i64 ptrtoint ([200 x i32]* @__msan_param_origin_tls to i64), i64 8) to i8*), i32 4, i1 false)
; CHECK:         store i64 0, i64* bitcast ([100 x i64]* @__msan_param_tls to i64*), align 8
; CHECK:         call void @Fn8(i8* %p)
; CHECK:         ret void
;
entry:
  call void @Fn8(i8* %p)
  ret void
}

define void @ByValForwardNoSanitize8(i32, i8* byval(i8) %p) {
; CHECK-LABEL: @ByValForwardNoSanitize8(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memset.p0i8.i32(i8* align 1 %[[#]], i8 0, i32 1, i1 false)
; CHECK:         store i64 0, i64* bitcast ([100 x i64]* @__msan_param_tls to i64*), align 8
; CHECK:         call void @Fn8(i8* %p)
; CHECK:         ret void
;
entry:
  call void @Fn8(i8* %p)
  ret void
}

define void @ByValForwardByVal8(i32, i8* byval(i8) %p) sanitize_memory {
; CHECK-LABEL: @ByValForwardByVal8(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 1 %[[#]], i8* align 1 inttoptr (i64 add (i64 ptrtoint ([100 x i64]* @__msan_param_tls to i64), i64 8) to i8*), i32 1, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %[[#]], i8* align 4 inttoptr (i64 add (i64 ptrtoint ([200 x i32]* @__msan_param_origin_tls to i64), i64 8) to i8*), i32 4, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* bitcast ([100 x i64]* @__msan_param_tls to i8*), i8* %[[#]], i32 1, i1 false)
; CHECK:         call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 bitcast ([200 x i32]* @__msan_param_origin_tls to i8*), i8* align 4 %[[#]], i32 4, i1 false)
; CHECK:         call void @FnByVal8(i8* byval(i8) %p)
; CHECK:         ret void
;
entry:
  call void @FnByVal8(i8* byval(i8) %p)
  ret void
}

define void @ByValForwardByValNoSanitize8(i32, i8* byval(i8) %p) {
; CHECK-LABEL: @ByValForwardByValNoSanitize8(
; CHECK-NEXT:  entry:
; CHECK:         call void @llvm.memset.p0i8.i32(i8* align 1 %[[#]], i8 0, i32 1, i1 false)
; CHECK:         call void @llvm.memset.p0i8.i32(i8* bitcast ([100 x i64]* @__msan_param_tls to i8*), i8 0, i32 1, i1 false)
; CHECK:         call void @FnByVal8(i8* byval(i8) %p)
; CHECK:         ret void
;
entry:
  call void @FnByVal8(i8* byval(i8) %p)
  ret void
}

