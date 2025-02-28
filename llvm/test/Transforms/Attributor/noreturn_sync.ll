; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function-signature --check-attributes --check-globals
; RUN: opt -attributor  -attributor-max-iterations-verify -attributor-annotate-decl-cs -attributor-max-iterations=2 -S < %s | FileCheck %s
;
; This file is the same as noreturn_async.ll but with a personality which
; indicates that the exception handler *cannot* catch asynchronous exceptions.
; As a consequence, invokes to noreturn and nounwind functions are translated
; to calls followed by an unreachable.
;
; https://reviews.llvm.org/D59978#inline-586873
;
; Make sure we handle invoke of a noreturn function correctly.
;
; This test is also a reminder of how we handle (=ignore) stackoverflow exception handling.
;
target datalayout = "e-m:w-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@"??_C@_0BG@CMNEKHOP@Exception?5NOT?5caught?6?$AA@" = linkonce_odr dso_local unnamed_addr constant [22 x i8] c"Exception NOT caught\0A\00", align 1
@"??_C@_0BC@NKPAGFFJ@Exception?5caught?6?$AA@" = linkonce_odr dso_local unnamed_addr constant [18 x i8] c"Exception caught\0A\00", align 1
@"??_C@_0BK@JHJLGDKL@Done?5execution?5result?$DN?$CFi?6?$AA@" = linkonce_odr dso_local unnamed_addr constant [26 x i8] c"Done execution result=%i\0A\00", align 1
@"?_OptionsStorage@?1??__local_stdio_printf_options@@9@4_KA" = linkonce_odr dso_local global i64 0, align 8


define dso_local void @"?overflow@@YAXXZ"() {
entry:
; CHECK-NOT:  Function Attrs:
; CHECK:      define
; CHECK-NEXT:   entry:
; CHECK-NEXT:   {{.*}}@printf{{.*}}
; CHECK-NEXT:   call void @"?overflow@@YAXXZ"()
; CHECK-NEXT:   {{.*}}@printf{{.*}}
; CHECK-NEXT:   ret void
  %call2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @"??_C@_0BC@NKPAGFFJ@Exception?5caught?6?$AA@", i64 0, i64 0)) nofree nosync nounwind
  call void @"?overflow@@YAXXZ"()
  %call3 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @"??_C@_0BC@NKPAGFFJ@Exception?5caught?6?$AA@", i64 0, i64 0))
  ret void
}


; CHECK-NOT:       Function Attrs:
; CHECK:   @"?catchoverflow@@YAHXZ"()
define dso_local i32 @"?catchoverflow@@YAHXZ"()  personality i8* bitcast (i32 (...)* @__gcc_personality_v0 to i8*) {
entry:
  %retval = alloca i32, align 4
  %__exception_code = alloca i32, align 4
  invoke void @"?overflow@@YAXXZ"()
  to label %invoke.cont unwind label %catch.dispatch
; CHECK:      invoke void @"?overflow@@YAXXZ"()
; CHECK-NEXT:        to label %invoke.cont unwind label %catch.dispatch

invoke.cont:                                      ; preds = %entry
  br label %invoke.cont1

catch.dispatch:                                   ; preds = %invoke.cont, %entry
  %0 = catchswitch within none [label %__except] unwind to caller

__except:                                         ; preds = %catch.dispatch
  %1 = catchpad within %0 [i8* null]
  catchret from %1 to label %__except2

__except2:                                        ; preds = %__except
  %2 = call i32 @llvm.eh.exceptioncode(token %1)
  store i32 1, i32* %retval, align 4
  br label %return

invoke.cont1:                                     ; preds = %invoke.cont
  store i32 0, i32* %retval, align 4
  br label %return

__try.cont:                                       ; No predecessors!
  store i32 2, i32* %retval, align 4
  br label %return

return:                                           ; preds = %__try.cont, %__except2, %invoke.cont1
  %3 = load i32, i32* %retval, align 4
  ret i32 %3
}


define dso_local void @"?overflow@@YAXXZ_may_throw"()  {
entry:
; CHECK-NOT:      Function Attrs:
; CHECK:      define
; CHECK-NEXT:   entry:
; CHECK-NEXT:   %call3 = call i32 (i8*, ...) @printf(i8* dereferenceable_or_null(18) getelementptr inbounds ([18 x i8], [18 x i8]* @"??_C@_0BC@NKPAGFFJ@Exception?5caught?6?$AA@", i64 0, i64 0))
; CHECK-NEXT:   call void @"?overflow@@YAXXZ_may_throw"()
; CHECK-NEXT:   ret void
  %call3 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @"??_C@_0BC@NKPAGFFJ@Exception?5caught?6?$AA@", i64 0, i64 0))
  call void @"?overflow@@YAXXZ_may_throw"()
  ret void
}


; CHECK-NOT:    Function Attrs:
; CHECK:        define
; CHECK-SAME:   @"?catchoverflow@@YAHXZ_may_throw"()
define dso_local i32 @"?catchoverflow@@YAHXZ_may_throw"()  personality i8* bitcast (i32 (...)* @__gcc_personality_v0 to i8*) {
entry:
  %retval = alloca i32, align 4
  %__exception_code = alloca i32, align 4
; CHECK: invoke void @"?overflow@@YAXXZ_may_throw"()
; CHECK:          to label %invoke.cont unwind label %catch.dispatch
  invoke void @"?overflow@@YAXXZ_may_throw"()
  to label %invoke.cont unwind label %catch.dispatch

invoke.cont:                                      ; preds = %entry
; CHECK:      invoke.cont:
; CHECK-NEXT: br label
  br label %invoke.cont1

catch.dispatch:                                   ; preds = %invoke.cont, %entry
  %0 = catchswitch within none [label %__except] unwind to caller

__except:                                         ; preds = %catch.dispatch
  %1 = catchpad within %0 [i8* null]
  catchret from %1 to label %__except2

__except2:                                        ; preds = %__except
  %2 = call i32 @llvm.eh.exceptioncode(token %1)
  store i32 1, i32* %retval, align 4
  br label %return

invoke.cont1:                                     ; preds = %invoke.cont
  store i32 0, i32* %retval, align 4
  br label %return

__try.cont:                                       ; No predecessors!
  store i32 2, i32* %retval, align 4
  br label %return

return:                                           ; preds = %__try.cont, %__except2, %invoke.cont1
  %3 = load i32, i32* %retval, align 4
  ret i32 %3
}

declare dso_local i32 @__gcc_personality_v0(...)

declare dso_local i32 @printf(i8* %_Format, ...)

declare i32 @llvm.eh.exceptioncode(token)
;.
; CHECK: attributes #[[ATTR0:[0-9]+]] = { nounwind readnone }
; CHECK: attributes #[[ATTR1:[0-9]+]] = { nofree nosync nounwind }
;.
