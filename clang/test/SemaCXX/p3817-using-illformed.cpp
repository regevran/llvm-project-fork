// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify %s

// P3817 (Structured Binding Assignments): cases the paper mandates as
// ill-formed. Each case here is a gap-documentation test: when the paper
// requires a diagnostic, this test enforces one so a regression can't
// silently reintroduce it.

struct Pair { int a, b; };
Pair get();

namespace StorageClass {
// A using-marked element assigns to an already-existing entity; there is
// no new entity for 'static'/'thread_local' to apply a storage class to.
void f() {
  int x;
  static auto [using x, y] = get();
  // expected-error@-1 {{structured binding declaration with a 'using'-marked element cannot be declared 'static'}}
  (void)y;
}

void g() {
  int x;
  thread_local auto [using x, y] = get();
  // expected-error@-1 {{structured binding declaration with a 'using'-marked element cannot be declared 'thread_local'}}
  (void)y;
}

// Non-using elements in the same list are unaffected when there is no
// using-marked element at all.
void ok() {
  static auto [x, y] = get();
  (void)x;
  (void)y;
}
} // namespace StorageClass
