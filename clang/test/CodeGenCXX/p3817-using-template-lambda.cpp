// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817: a using-marked binding inside a lambda nested in a template body
// must still perform its reused assignment once the template is
// instantiated. Verified this needs no code of its own beyond the
// VisitBindingDecl fix for the plain case (see
// CodeGenCXX/p3817-using-template.cpp) -- a lambda's call operator body is
// instantiated through the same TemplateDeclInstantiator/TreeTransform
// machinery as any other function body, so there was never a separate path
// to fix. This is a regression guard for that, not a new code path.

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

template<class T> void goo(T &t) {
  auto lambda = [&]() {
    auto [using t, y] = getPair();
    (void)y;
  };
  lambda();
}

// CHECK-LABEL: define {{.*}}@_ZZ3gooI7TrackerEvRT_ENKUlvE_clEv(
template void goo<Tracker>(Tracker&);
// CHECK: call {{.*}}@_ZN7TrackeraSEOS_(
