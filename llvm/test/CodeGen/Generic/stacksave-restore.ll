; RUN: llc < %s

target triple = "x86_64-unknown-linux"

declare i8* @llvm.stacksave()

declare void @llvm.stackrestore(i8*)

define i32* @test(i32 %N) {
        %tmp = call i8* @llvm.stacksave( )              ; <i8*> [#uses=1]
        %P = alloca i32, i32 %N         ; <i32*> [#uses=1]
        call void @llvm.stackrestore( i8* %tmp )
        %Q = alloca i32, i32 %N         ; <i32*> [#uses=0]
        ret i32* %P
}

