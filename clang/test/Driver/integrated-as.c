// RUN: %clang -target x86_64-unknown-linux -### -c -save-temps -integrated-as %s 2>&1 | FileCheck %s

// CHECK: cc1as
// CHECK: -mrelax-all

// RUN: %clang -target x86_64-unknown-linux -### -fintegrated-as -c -save-temps %s 2>&1 | FileCheck %s -check-prefix FIAS

// FIAS: cc1as

// RUN: %clang -target x86_64-unknown-linux -target none -### -fno-integrated-as -S %s 2>&1 \
// RUN:     | FileCheck %s -check-prefix NOFIAS

// NOFIAS-NOT: cc1as
// NOFIAS: -cc1
// NOFIAS: "-fno-verbose-asm"
// NOFIAS: -no-integrated-as
