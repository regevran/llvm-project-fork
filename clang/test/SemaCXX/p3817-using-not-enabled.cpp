// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify %s

// P3817 (Structured Binding Assignments): an experimental, non-standard
// extension -- 'using' in a structured binding declaration must be
// rejected unless explicitly enabled via -fstructured-binding-assignment,
// regardless of -std= mode.

struct Pair { int a, b; };
Pair get();

void f() {
  int x;
  auto [using x, y] = get();
  // expected-error@-1 {{'using' in a structured binding declaration is not enabled; use '-fstructured-binding-assignment' to enable this experimental extension}}
  // expected-error@-2 {{type 'Pair' binds to 2 elements, but no names were provided}}
  (void)y; // expected-error {{use of undeclared identifier 'y'}}
}

// An ordinary (non-using) structured binding is unaffected.
void ok() {
  auto [a, b] = get();
  (void)a;
  (void)b;
}
