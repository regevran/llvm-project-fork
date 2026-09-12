// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817: a using-marked binding inside a template body must still perform
// its reused assignment once the template is instantiated.
// TemplateDeclInstantiator::VisitBindingDecl used to clone a BindingDecl by
// copying only its identifier and type, silently dropping the using-marked
// target expression -- so `using` bindings inside templates did nothing at
// all post-instantiation (no assignment, no diagnostic). Fixed by
// substituting and propagating ReusedTargetExpr the same way any other
// expression referencing template parameters or earlier locals gets
// substituted during instantiation; the reused assignment itself
// (ReusedAssignment) doesn't need to be cloned -- it's rebuilt from scratch
// by the same completion pass (BuildP3817ReusedAssignments) that already
// runs for a non-template decomposition.

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

// The using-target `t` has a template-dependent type (T&), but the
// decomposed type (Pair, from getPair()) does not -- so this specifically
// exercises a using-target that's only resolved at instantiation, inside an
// otherwise-non-dependent decomposition.
template<class T> void goo(T &t) {
  auto [using t, y] = getPair();
  (void)y;
}

// CHECK-LABEL: define {{.*}}@_Z3gooI7TrackerEvRT_(
template void goo<Tracker>(Tracker&);
// CHECK: call {{.*}}@_ZN7TrackeraSEOS_(
