// RUN: %clang -target x86_64-unknown-none-none -c -fproc-stat-report -fintegrated-as %s | FileCheck %s
// CHECK: clang{{.*}}: output={{.*}}.o, total={{[0-9.]+}} ms, user={{[0-9.]+}} ms, mem={{[0-9]+}} Kb

// RUN: %clang -target x86_64-unknown-none-none -c -fintegrated-as -fproc-stat-report=%t %s
// RUN: cat %t | FileCheck --check-prefix=CSV %s
// CSV: clang{{.*}},"{{.*}}.o",{{[0-9]+}},{{[0-9]+}},{{[0-9]+}}
