// RUN: %clang_cc1 -ast-dump -std=c++2c %s | FileCheck %s

// P3817 (Structured Binding Assignments): a using-marked binding declares
// nothing (it has no name), so its dump must show something else to make it
// recognizable -- the `using` marker, its target, and the built
// `target = source` assignment -- rather than looking like an anonymous
// ordinary binding with just a tuple/member "Binding" child.

struct Pair { int a, b; };
Pair get();

void f() {
  int x = 0;
  auto [using x, y] = get();
  (void)y;
}

// CHECK-LABEL: FunctionDecl {{.*}} f
// CHECK: BindingDecl {{.*}} 'int' using
// CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue Var {{.*}} 'x' 'int'
// CHECK-NEXT: BinaryOperator {{.*}} 'int' lvalue '='
// CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue Var {{.*}} 'x' 'int'
// CHECK: MemberExpr {{.*}} 'int' lvalue .a {{.*}}
// CHECK-NEXT: DeclRefExpr {{.*}} 'Pair' lvalue Decomposition {{.*}} first_binding
// CHECK-NOT: using
// CHECK: BindingDecl {{.*}} y 'int'
// CHECK-NEXT: MemberExpr {{.*}} 'int' lvalue .b {{.*}}
