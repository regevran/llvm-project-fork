// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fsyntax-only -verify %s

// P3817R1 added a normative left-to-right assignment order guarantee across
// all three decomposition kinds (see the "Order of assignment" paragraph
// under "Semantics" in P3817.md): SBi's assignment (if using-marked) must
// fully complete before SBi+1 is evaluated. This is only observable when a
// using-marked element's right-hand side aliases storage that an earlier
// using-marked element also writes to -- the paper's own example is an
// "evict-and-shift" over an array: `auto& [_, using arr[0], using arr[1]] =
// arr;`. Under the correct (left-to-right) order this shifts elements left
// and duplicates the last one; under the wrong (right-to-left) order the
// first write reads a value the second write already clobbered, silently
// corrupting the array instead (losing the middle element entirely).
//
// Each case below is checked via `static_assert` on a `constexpr` function,
// so a wrong-order regression is a hard compile error, not just a wrong
// runtime value: the correct final state is asserted; the (undesired)
// wrong-order outcome each case would produce instead is noted in a comment
// alongside it, rather than a second assertion, since compiling and
// executing the reverse order is not something these source-level tests can
// demonstrate directly.

// expected-no-diagnostics

namespace std {
  using size_t = decltype(sizeof(0));
  template<typename> struct tuple_size;
  template<size_t, typename> struct tuple_element;
}

// -- Array decomposition: the paper's own "evict-and-shift" example. --

constexpr bool testArrayOrder() {
  int arr[3] = {5, 8, 13};
  auto &[_, using arr[0], using arr[1]] = arr;
  // Left-to-right: arr[0] = arr[1] (8) first, giving {8, 8, 13}; then
  // arr[1] = arr[2] (13), giving {8, 13, 13}.
  // Right-to-left would instead give {13, 13, 13} (8 is lost: arr[1] is
  // overwritten with 13 before arr[0] reads it).
  return arr[0] == 8 && arr[1] == 13 && arr[2] == 13;
}
static_assert(testArrayOrder());

// -- Aggregate (member) decomposition: same shape, over struct members. --

struct Triple { int a, b, c; };

constexpr bool testAggregateOrder() {
  Triple t{21, 34, 55};
  auto &[_, using t.a, using t.b] = t;
  // Left-to-right: {34, 55, 55}. Right-to-left would give {55, 55, 55}.
  return t.a == 34 && t.b == 55 && t.c == 55;
}
static_assert(testAggregateOrder());

// -- Tuple-like decomposition: the paper notes this ordering already comes
// "for free" from existing per-binding holding-var sequencing ([dcl.struct.
// bind]p7), unlike the array/aggregate cases above -- included as a
// regression guard for that claim, using a get<N> that aliases a shared
// backing store so a wrong order would be observable the same way. --

struct TupleLike { int *data; };
template<> struct std::tuple_size<TupleLike> { enum { value = 3 }; };
template<> struct std::tuple_element<0, TupleLike> { using type = int; };
template<> struct std::tuple_element<1, TupleLike> { using type = int; };
template<> struct std::tuple_element<2, TupleLike> { using type = int; };
template<int N> constexpr int &get(TupleLike t) { return t.data[N]; }

constexpr bool testTupleLikeOrder() {
  int arr[3] = {89, 144, 233};
  TupleLike tl{arr};
  auto &[_, using arr[0], using arr[1]] = tl;
  // Left-to-right: {144, 233, 233}. Right-to-left would give {233, 233, 233}.
  return arr[0] == 144 && arr[1] == 233 && arr[2] == 233;
}
static_assert(testTupleLikeOrder());
