// RUN: %clang_cc1 -std=c++20 -ast-print %s | FileCheck %s

// The `[a, b]` binding list must survive -ast-print, not just the type.

namespace std {
using size_t = decltype(sizeof(0));
template <typename> struct tuple_size;
template <size_t, typename> struct tuple_element;
} // namespace std

namespace Aggregate {
struct Pair { int a, b; };
Pair get();
Pair &getref();

// CHECK-LABEL: void local() {
void local() {
  // CHECK-NEXT: auto [x, y] = get();
  auto [x, y] = get();
  // CHECK-NEXT: auto & [rx, ry] = getref();
  auto &[rx, ry] = getref();
  // CHECK-NEXT: const auto [cx, cy] = get();
  const auto [cx, cy] = get();
  // CHECK-NEXT: static auto [sx, sy] = get();
  static auto [sx, sy] = get();
}
} // namespace Aggregate

namespace Array {
// CHECK-LABEL: void local() {
void local() {
  // CHECK-NEXT: int arr[3] = {1, 2, 3};
  int arr[3] = {1, 2, 3};
  // CHECK-NEXT: auto [a, b, c]
  auto [a, b, c] = arr;
}
} // namespace Array

namespace TupleLike {
struct Two {};
Two getTwo();
} // namespace TupleLike

template <> struct std::tuple_size<TupleLike::Two> { enum { value = 2 }; };
template <> struct std::tuple_element<0, TupleLike::Two> { using type = int; };
template <> struct std::tuple_element<1, TupleLike::Two> { using type = int; };

namespace TupleLike {
// get() must be found by ADL, so it needs to live here, not at global scope.
template <std::size_t N> int get(Two);

// CHECK-LABEL: void local() {
void local() {
  // CHECK-NEXT: auto [p, q] = getTwo();
  auto [p, q] = getTwo();
}
} // namespace TupleLike

namespace NamespaceScope {
using Aggregate::Pair;
using Aggregate::get;

// CHECK-NOT: {{^[[:space:]]*;[[:space:]]*$}}
// CHECK: auto [gx, gy] = get();
auto [gx, gy] = get();
// CHECK-NOT: {{^[[:space:]]*;[[:space:]]*$}}
} // namespace NamespaceScope
