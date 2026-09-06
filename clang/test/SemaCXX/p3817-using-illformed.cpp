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

namespace UsingPlaceholder {
// '_' is the discard placeholder; it can't be a using-target, regardless
// of whether some entity named '_' happens to be in scope -- the plain
// declaration form never performs lookup for '_' either, it just always
// declares a fresh placeholder.
void no_placeholder_in_scope() {
  auto [using _, y] = get();
  // expected-error@-1 {{'using' target cannot be the placeholder '_'}}
  // expected-error@-2 {{type 'Pair' binds to 2 elements, but no names were provided}}
  (void)y; // expected-error {{use of undeclared identifier 'y'}}
}

// The dangerous case: a lone placeholder resolves unambiguously via
// ordinary lookup (Sema only diagnoses '_' as a *reference* when it's
// ambiguous between multiple placeholders), so this must be rejected
// before it ever reaches ordinary expression Sema.
void one_placeholder_in_scope() {
  auto [_, y] = get();
  auto [using _, z] = get();
  // expected-error@-1 {{'using' target cannot be the placeholder '_'}}
  // expected-error@-2 {{type 'Pair' binds to 2 elements, but no names were provided}}
  (void)y;
  (void)z; // expected-error {{use of undeclared identifier 'z'}}
}

// '_' as an ordinary (non-using) discard element is unaffected.
void ok_placeholder_element() {
  auto [x, _] = get();
  (void)x;
}
} // namespace UsingPlaceholder

namespace UsingAttribute {
// No attribute-specifier-seq appears in the using-marked alternative of
// sb-identifier -- attributes appertain to a newly declared variable, and
// a using-marked element introduces none.
void attr_on_using_element() {
  int x;
  auto [using x [[maybe_unused]], y] = get();
  // expected-error@-1 {{an attribute list cannot appear here}}
  // expected-error@-2 {{type 'Pair' binds to 2 elements, but no names were provided}}
  (void)y; // expected-error {{use of undeclared identifier 'y'}}
}

// An attribute on an ordinary (non-using) element is unaffected.
void attr_on_plain_element_ok() {
  auto [x [[maybe_unused]], y] = get();
  (void)x;
  (void)y;
}

// A using-marked element without an attribute is unaffected.
void using_without_attr_ok() {
  int x;
  auto [using x, y] = get();
  (void)y;
}
} // namespace UsingAttribute
