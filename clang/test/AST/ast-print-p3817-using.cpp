// RUN: %clang_cc1 -ast-print -std=c++2c %s | FileCheck %s

// P3817 (Structured Binding Assignments): a using-marked binding has no
// name of its own -- print its target instead, so `auto [using x, y]`
// round-trips instead of losing the `using x` element entirely.

struct Pair { int a, b; };
Pair get();

void f() {
  int x = 0;
  auto [using x, y] = get();
  (void)y;
}

// CHECK: void f() {
// CHECK-NEXT: int x = 0;
// CHECK-NEXT: auto [using x, y] = get();

struct S { int m; };
void g(S &s) {
  auto [using s.m, y] = get();
  (void)y;
}

// CHECK: void g(S &s) {
// CHECK-NEXT: auto [using s.m, y] = get();
