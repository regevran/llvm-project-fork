// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++2c -fsyntax-only -verify %s -fexperimental-new-constant-interpreter

// P3817 (Structured Binding Assignments): `using` before a
// structured-binding element marks it as an assignment target -- an
// existing lvalue -- rather than a new declaration, e.g.
// `auto [using x, y] = f();` assigns to x and declares y.

namespace Basic {
struct Pair { int a, b; };
Pair get();

void local_var() {
  int x = 0;
  auto [using x, y] = get();
  (void)y;
}

void enclosing_scope() {
  int x = 0;
  {
    auto [using x, y] = get();
    (void)y;
  }
}

// A using-target may itself be an earlier structured-binding element.
void earlier_binding() {
  auto [p, q] = get();
  auto [using p, using q] = get();
}
} // namespace Basic

namespace Member {
// A using-target that's a non-static data member gets an implicit `this->`.
struct S {
  int x = 0;
  void update(Basic::Pair in) {
    auto [using x, y] = in;
    (void)y;
  }
  static void bad(Basic::Pair in) {
    auto [using x, y] = in; // expected-error {{invalid use of member 'x' in static member function}} \
                            // expected-error {{type 'Basic::Pair' binds to 2 elements, but no names were provided}}
    (void)y; // expected-error {{use of undeclared identifier 'y'}}
  }
};
} // namespace Member

namespace Diagnostics {
using Basic::Pair;
using Basic::get;

void undeclared_target() {
  auto [using nope, y] = get(); // expected-error {{use of undeclared identifier 'nope'}} \
                                // expected-error {{type 'Pair' binds to 2 elements, but no names were provided}}
  (void)y; // expected-error {{use of undeclared identifier 'y'}}
}

void const_target() {
  const int cx = 0; // expected-note {{variable 'cx' declared const here}}
  auto [using cx, y] = get(); // expected-error {{cannot assign to variable 'cx' with const-qualified type 'const int'}}
  (void)y;
}
} // namespace Diagnostics

namespace CommaDisambiguation {
using Basic::Pair;
using Basic::get;

// `using a, b` must parse as two bindings (using a; new decl b), not a
// single using-target formed from the comma operator `(a, b)`. Restricting
// the grammar to unary-expression (rather than a broader expression
// grammar) is what guarantees this: the comma operator only exists at the
// `expression` production, several levels above unary-expression, with no
// path back up to it without explicit parentheses.
void two_bindings_not_one() {
  int a = 0;
  auto [using a, b] = get();
  (void)b;
}
} // namespace CommaDisambiguation

namespace ConstantEvaluation {
using Basic::Pair;

// The built `target = source` assignment must run as a side effect during
// constant evaluation too, not just at runtime: CodeGen and the constant
// evaluator each have their own deferred per-binding-init step, and only
// teaching one of them about using-marked bindings would silently make `a`
// below evaluate as if the assignment never happened, instead of failing to
// compile.
constexpr int reassigns_target(Pair p) {
  int a = 0;
  auto [using a, b] = p;
  return a * 10 + b;
}
static_assert(reassigns_target({1, 2}) == 12);
} // namespace ConstantEvaluation
