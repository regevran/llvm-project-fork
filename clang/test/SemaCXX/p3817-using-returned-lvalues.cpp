// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fsyntax-only -verify %s
// expected-no-diagnostics

// P3817's "Returned Lvalues": the target of a using-marked binding is a
// unary-expression, not just a bare identifier -- it may be any expression
// that designates a modifiable lvalue: a subscript, a function call result,
// a member access, or a chain of these.

struct Pair { int a, b; };
Pair get();

void subscript_target() {
  int s[2] = {0, 0};
  auto [using s[0], y] = get();
  (void)y;
}

int& func() { static int storage = 0; return storage; }

void call_target() {
  auto [using func(), y] = get();
  (void)y;
}

struct Point { int x, y; };
void member_target() {
  Point p{0, 0};
  auto [using p.x, y] = get();
  (void)y;
}

struct Map {
  int& operator[](const char*);
};
void nested_subscript_target() {
  Map m;
  auto [using m["k"], y] = get();
  (void)y;
}
