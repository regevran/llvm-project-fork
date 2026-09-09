// RUN: %clang_cc1 -ast-dump -std=c++2c -fstructured-binding-assignment %s | FileCheck %s

// P3817 (Structured Binding Assignments): a using-marked target is parsed as
// a unary-expression, which never itself swallows a following comma --
// `using a, b` must parse as two separate binding-list elements
// ("using a" then "b"), never as one using-target built from the comma
// operator on "a, b" (which would need exactly one name for a two-element
// Pair, a compile error, not a silent wrong-value bug). This is the
// grammar-level guarantee; p3817_test/comma_disambiguation_test.cpp checks
// the same guarantee at runtime.
//
// Verified here regardless of which binding-list position is using-marked:
// first, second, or both -- each must still produce two BindingDecls.

struct Pair { int a, b; };
Pair get();

// CHECK-LABEL: FunctionDecl {{.*}} using_first
void using_first() {
  int a = 0;
  // CHECK: BindingDecl {{.*}} 'int' using
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue Var {{.*}} 'a' 'int'
  // CHECK: BindingDecl {{.*}} b 'int'
  auto [using a, b] = get();
  (void)b;
}

// CHECK-LABEL: FunctionDecl {{.*}} using_second
void using_second() {
  int b = 0;
  // CHECK: BindingDecl {{.*}} a 'int'
  // CHECK: BindingDecl {{.*}} 'int' using
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue Var {{.*}} 'b' 'int'
  auto [a, using b] = get();
  (void)a;
}

// CHECK-LABEL: FunctionDecl {{.*}} using_both
void using_both() {
  int a = 0, b = 0;
  // CHECK: BindingDecl {{.*}} 'int' using
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue Var {{.*}} 'a' 'int'
  // CHECK: BindingDecl {{.*}} 'int' using
  // CHECK-NEXT: DeclRefExpr {{.*}} 'int' lvalue Var {{.*}} 'b' 'int'
  auto [using a, using b] = get();
}
