// RUN: %clang_cc1 -triple=x86_64-apple-darwin  -emit-llvm -o - %s | FileCheck %s
// rdar://11861085

struct s {
  char filler [128];
  volatile int x;
};

struct s gs;

void foo (void) {
  struct s ls;
  ls = ls;
  gs = gs;
  ls = gs;
}
// CHECK-LABEL: define{{.*}} void @foo()
// CHECK: %[[LS:.*]] = alloca %struct.s, align 4
// CHECK-NEXT: %[[ZERO:.*]] = bitcast %struct.s* %[[LS]] to i8*
// CHECK-NEXT: %[[ONE:.*]] = bitcast %struct.s* %[[LS]] to i8*
// CHECK-NEXT: call void @llvm.memcpy.{{.*}}(i8* align 4 %[[ZERO]], i8* align 4 %[[ONE]], i64 132, i1 true)
// CHECK-NEXT: call void @llvm.memcpy.{{.*}}(i8* align 4 bitcast (%struct.s* @gs to i8*), i8* align 4 bitcast (%struct.s* @gs to i8*), i64 132, i1 true)
// CHECK-NEXT: %[[TWO:.*]] = bitcast %struct.s* %[[LS]] to i8*
// CHECK-NEXT: call void @llvm.memcpy.{{.*}}(i8* align 4 %[[TWO]], i8* align 4 bitcast (%struct.s* @gs to i8*), i64 132, i1 true)


struct s1 {
  struct s y;
};

struct s1 s;

void fee (void) {
  s = s;
  s.y = gs;
}
// CHECK-LABEL: define{{.*}} void @fee()

