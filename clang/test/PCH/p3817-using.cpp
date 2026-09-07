// RUN: %clang_cc1 -std=c++2c -include %s -verify %s
// RUN: %clang_cc1 -std=c++2c -include %s -verify %s -fexperimental-new-constant-interpreter
//
// RUN: %clang_cc1 -std=c++2c -emit-pch %s -o %t
// RUN: %clang_cc1 -std=c++2c -include-pch %t -verify %s
// RUN: %clang_cc1 -std=c++2c -emit-pch %s -o %t -fexperimental-new-constant-interpreter
// RUN: %clang_cc1 -std=c++2c -include-pch %t -verify %s -fexperimental-new-constant-interpreter

// expected-no-diagnostics

// P3817 (Structured Binding Assignments): BindingDecl::ReusedTargetExpr and
// ReusedAssignment must survive a PCH boundary. reassigns_target is defined
// in the header half below (built into the PCH) and only called from the
// `#else` half (as if downstream of the PCH boundary), so this fails if
// either field silently deserializes as null instead of the real
// using-target/assignment expressions.

#ifndef HEADER
#define HEADER

struct Pair { int a, b; };
constexpr int reassigns_target(Pair p) {
  int a = 0;
  auto [using a, b] = p;
  return a * 10 + b;
}

#else

static_assert(reassigns_target({1, 2}) == 12);

#endif
