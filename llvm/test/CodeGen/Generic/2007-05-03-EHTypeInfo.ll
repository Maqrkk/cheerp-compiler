; RUN: llc < %s

target triple = "x86_64-unknown-linux"

	%struct.exception = type { i8, i8, i32, i8*, i8*, i32, i8* }
@program_error = external global %struct.exception		; <%struct.exception*> [#uses=1]

define void @typeinfo() {
entry:
	%eh_typeid = tail call i32 @llvm.eh.typeid.for( i8* bitcast (%struct.exception* @program_error to i8*) )		; <i32> [#uses=0]
	ret void
}

declare i32 @llvm.eh.typeid.for(i8*)
