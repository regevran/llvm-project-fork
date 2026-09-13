// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fsyntax-only -verify %s -fexperimental-new-constant-interpreter

// expected-no-diagnostics

// P3817: a using-marked binding's target must be re-derived at each template
// instantiation (see CodeGenCXX/p3817-using-template.cpp) *and* its
// assignment must actually run during constant evaluation (see
// ConstantEvaluation in p3817-using.cpp) -- this is the combination of both:
// a using-marked binding inside a template, evaluated in a constant
// expression. Neither fix alone is exercised by the other's test.
constexpr int ar[2] = {42, 43};

template <class T>
constexpr int foo() {
  T a = 0;
  auto [using a, b] = ar;
  return a * 10 + b;
}

static_assert(foo<int>() == 463);
