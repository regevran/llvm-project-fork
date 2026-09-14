// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817: a using-pack (`using ...targets`, "Case 1" -- targets must already
// denote a pack, e.g. a function parameter pack; see REFERENCE-P3817.md's
// "Packs" entries for the deferred "Case 2") reuses each element of that
// pack as one of this structured binding's targets, one position per pack
// element. Verifies the real per-element assignment (via operator=, not a
// raw store) actually runs -- see SemaCXX/p3817-using-pack-illformed.cpp
// for the pack-specific ill-formed cases this doesn't cover.

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

template<class... Ts> void assign_all(Ts&... targets) {
  auto [using ...targets] = getPair();
}

// CHECK-LABEL: define {{.*}}@_Z10assign_allIJ7TrackerS0_EEvDpRT_(
template void assign_all<Tracker, Tracker>(Tracker&, Tracker&);
// CHECK: call {{.*}}@_ZN7TrackeraSEOS_(
// CHECK: call {{.*}}@_ZN7TrackeraSEOS_(
