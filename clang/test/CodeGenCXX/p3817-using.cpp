// RUN: %clang_cc1 -std=c++2c -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817: a using-marked binding must be emitted as a real call through the
// class's actual operator= (not a raw store), selecting move vs copy
// according to the ref-qualifier: `auto` -> move (the hidden variable e is
// an xvalue), `auto&`/`auto&&` bound to an lvalue -> copy. Verified for both
// decomposition paths: member decomposition (no holding var) and tuple-like
// decomposition via std::get (which does use a holding var -- and where
// using-marked bindings previously did nothing at all, silently).

namespace std {
  using size_t = decltype(sizeof(0));
  template<typename> struct tuple_size;
  template<size_t, typename> struct tuple_element;
}

struct Tracker {
  int value;
  Tracker(int v) : value(v) {}
  Tracker(const Tracker&);
  Tracker(Tracker&&);
  Tracker &operator=(const Tracker&);
  Tracker &operator=(Tracker&&);
};

// -- Member decomposition (no holding var). --

struct Pair { Tracker a, b; };
Pair getPair();

// CHECK-LABEL: define {{.*}}@_Z16array_auto_movesR7Tracker(
void array_auto_moves(Tracker &t) {
  auto [using t, y] = getPair();
  // CHECK: call {{.*}}@_ZN7TrackeraSEOS_(
}

// CHECK-LABEL: define {{.*}}@_Z21array_auto_ref_copiesR7TrackerR4Pair(
void array_auto_ref_copies(Tracker &t, Pair &p) {
  auto &[using t, y] = p;
  // CHECK: call {{.*}}@_ZN7TrackeraSERKS_(
}

// -- Tuple-like decomposition (with holding var). --

struct TupleLike {};
template<> struct std::tuple_size<TupleLike> { enum { value = 2 }; };
template<> struct std::tuple_element<0,TupleLike> { using type = Tracker; };
template<> struct std::tuple_element<1,TupleLike> { using type = Tracker; };
template<int N> Tracker get(TupleLike);
TupleLike getTupleLike();

// CHECK-LABEL: define {{.*}}@_Z16tuple_auto_movesR7Tracker(
void tuple_auto_moves(Tracker &t) {
  auto [using t, y] = getTupleLike();
  // CHECK: call {{.*}}@_ZN7TrackeraSEOS_(
}

// CHECK-LABEL: define {{.*}}@_Z21tuple_auto_ref_copiesR7TrackerR9TupleLike(
void tuple_auto_ref_copies(Tracker &t, TupleLike &tl) {
  auto &[using t, y] = tl;
  // CHECK: call {{.*}}@_ZN7TrackeraSERKS_(
}
