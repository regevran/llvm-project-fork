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

namespace DuplicateTarget {
// Using the same variable more than once in a using-marked binding list is
// ill-formed, even for a type whose operator= would tolerate it.
void simple() {
  int x;
  auto [using x, using x] = get();
  // expected-error@-1 {{'using' target already appears earlier in this structured binding declaration}}
  // expected-note@-2 {{previous 'using' target specified here}}
}

struct S { int m; };
void member() {
  S s;
  auto [using s.m, using s.m] = get();
  // expected-error@-1 {{'using' target already appears earlier in this structured binding declaration}}
  // expected-note@-2 {{previous 'using' target specified here}}
}

void subscript() {
  int arr[2];
  auto [using arr[0], using arr[0]] = get();
  // expected-error@-1 {{'using' target already appears earlier in this structured binding declaration}}
  // expected-note@-2 {{previous 'using' target specified here}}
}

// A different constant index is a different lvalue -- not a duplicate.
void distinct_subscript() {
  int arr[2];
  auto [using arr[0], using arr[1]] = get();
}

// Two calls need not return the same lvalue, so this is conservatively not
// flagged -- unlike a plain variable, a function call isn't provably the
// same entity each time it's written.
int& ref();
void distinct_calls_not_flagged() {
  auto [using ref(), using ref()] = get();
}
} // namespace DuplicateTarget
