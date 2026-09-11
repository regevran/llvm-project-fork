// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fstructured-binding-assignment-allow-const -fsyntax-only -verify %s

// P3817 (Structured Binding Assignments): the paper leaves 'const'/'constexpr'
// on a using-containing structured binding ill-formed (see the "Alternative
// Considered" under #const in P3817.md) -- but the alternative it rejects is
// already fully implemented and correct here (const only applies to the
// hidden decomposed object, so a using-marked target's own type is
// untouched, and the reused assignment correctly selects copy instead of
// move -- see CodeGenCXX/p3817-using-allow-const.cpp for that check).
// -fstructured-binding-assignment-allow-const opts into it. See
// p3817-using-illformed.cpp's ConstQualifier namespace for the default
// (rejecting) behavior this flag turns off.

// expected-no-diagnostics

struct Pair { int a, b; };
Pair get();
constexpr Pair getConstexpr() { return {1, 2}; }

void f() {
  int x;
  const auto [using x, y] = get();
  (void)y;
}

void g() {
  int x;
  constexpr auto [using x, y] = getConstexpr();
  (void)y;
}
