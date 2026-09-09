// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817: a using-marked binding's reused assignment must run for a
// namespace-scope decomposition too, not just a local one (see
// CodeGenFunction::EmitCXXGlobalVarDeclInit). Verified for all three
// decomposition kinds. For the tuple-like kind (the only one with a
// holding var), also verifies a using-marked binding's holding var is
// initialized inside the *same* ctor as the DecompositionDecl itself,
// immediately before the assignment that reads through it, while a
// non-using-marked binding's holding var keeps its own separate top-level
// ctor exactly as it does without this feature.

// -- Aggregate decomposition (no holding var). --
namespace Aggregate {
struct Pair { int a, b; };
Pair getPair();

int gx;
// CHECK-LABEL: define internal void @__cxx_global_var_init()
// CHECK: call {{.*}}getPair
// CHECK: store i32 {{.*}}@_ZN9Aggregate2gxE
auto [using gx, y] = getPair();
} // namespace Aggregate

// -- Array decomposition (no holding var). --
namespace Array {
int arr[2] = {1, 2};

int gx;
// CHECK-LABEL: define internal void @__cxx_global_var_init.1()
// CHECK: store i32 {{.*}}@_ZN5Array2gxE
auto [using gx, y] = arr;
} // namespace Array

// -- Tuple-like decomposition (with holding var). --
namespace std {
  using size_t = decltype(sizeof(0));
  template<typename> struct tuple_size;
  template<size_t, typename> struct tuple_element;
}

namespace TupleLike {
struct TL {};
} // namespace TupleLike
template<> struct std::tuple_size<TupleLike::TL> { enum { value = 2 }; };
template<> struct std::tuple_element<0,TupleLike::TL> { using type = int; };
template<> struct std::tuple_element<1,TupleLike::TL> { using type = int; };
namespace TupleLike {
template<int N> int get(TL);
TL getTL();

int gx;
// The using-marked binding's holding var (get<0>) must be initialized
// *inside* this same ctor, before the assignment that reads through it.
// CHECK-LABEL: define internal void @__cxx_global_var_init.2()
// CHECK: call {{.*}}getTL
// CHECK: call {{.*}}getILi0E{{.*}}
// CHECK: store i32 {{.*}}@_ZN9TupleLike2gxE

// A non-using-marked binding's holding var still gets its own,
// independent ctor -- unaffected by this feature.
// CHECK-LABEL: define internal void @__cxx_global_var_init.3()
// CHECK: call {{.*}}getILi1E{{.*}}
// CHECK: store i32 {{.*}}@_ZN9TupleLike1yE
auto [using gx, y] = getTL();
} // namespace TupleLike
