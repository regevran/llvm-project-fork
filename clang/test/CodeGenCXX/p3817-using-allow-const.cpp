// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fstructured-binding-assignment-allow-const -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817: with -fstructured-binding-assignment-allow-const, a using-marked
// binding inside a const structured binding is accepted (see
// p3817-using-illformed.cpp's ConstQualifier namespace for the
// default-rejecting behavior this flag opts out of), and the reused
// assignment correctly selects copy, not move -- const applies to the
// hidden decomposed object e, so e's members are const lvalues (never
// xvalues), and a const lvalue can only bind to operator=(const Tracker&).
// This is exactly the "Alternative Considered" behavior documented under
// #const in P3817.md.

struct Tracker {
  int value;
  Tracker(int v) : value(v) {}
  Tracker(const Tracker&);
  Tracker(Tracker&&);
  Tracker &operator=(const Tracker&);
  Tracker &operator=(Tracker&&);
};

struct Pair { Tracker a, b; };
Pair getPair();

// CHECK-LABEL: define {{.*}}@_Z17const_auto_copiesR7Tracker(
void const_auto_copies(Tracker &t) {
  const auto [using t, y] = getPair();
  // CHECK: call {{.*}}@_ZN7TrackeraSERKS_(
  (void)y;
}
