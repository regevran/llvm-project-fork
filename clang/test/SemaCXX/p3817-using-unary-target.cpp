// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fsyntax-only -verify %s

// P3817 (Structured Binding Assignments): ParseDecompositionDeclarator's
// disambiguation heuristic (structured-binding-list vs. misplaced-array-
// declarator) must not reject a prefix unary-operator using-target --
// `using` is a keyword, so no lookahead disambiguation is even needed for
// it in the first place (unlike a bare identifier or an ellipsis, which
// really can start an array-bound expression). Fixed in 1678e1bef254.

struct Pair { int a, b; };
Pair get();

namespace AcceptedByParser {
// A prefix unary operator applied to an lvalue yields an lvalue -- these
// parse and compile cleanly; the fix is purely about the parser no longer
// rejecting them before Sema even sees them.
void increment() {
  int x = 0;
  auto [using ++x, y] = get();
  (void)y;
}

void decrement() {
  int x = 5;
  auto [using --x, y] = get();
  (void)y;
}

void dereference() {
  int x = 0;
  int *p = &x;
  auto [using *p, y] = get();
  (void)y;
}
} // namespace AcceptedByParser

namespace RejectedBySemaNotParser {
// These parse fine too (same fix), but are correctly rejected afterward,
// by Sema's ordinary assignability check on the built `target = source`
// expression -- the same diagnostic any plain `-x = ...` or `&x = ...`
// would get. The parser's job is only to not pre-empt Sema here.
void negation() {
  int x = 0;
  auto [using -x, y] = get();
  // expected-error@-1 {{expression is not assignable}}
  (void)y;
}

void address_of() {
  int x = 0;
  auto [using &x, y] = get();
  // expected-error@-1 {{expression is not assignable}}
  (void)y;
}
} // namespace RejectedBySemaNotParser
