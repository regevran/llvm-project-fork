// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck %s

// P3817 (Structured Binding Assignments): a namespace-scope decomposition
// declaration needs a real mangled name for its hidden decomposed object,
// and Clang's DC<source-name>*E scheme (ItaniumMangle.cpp) builds that name
// from each binding's own name. A using-marked binding has no name of its
// own -- mangleSourceName crashed on the null identifier before this was
// fixed to mangle the using-target expression instead, for every kind of
// target the grammar allows.

int ar[2] = {1, 2};

int gx;
// CHECK: @_ZDC2y1L_Z2gxEE = global [2 x i32] zeroinitializer
auto [y1, using gx] = ar;

struct S { int m; };
S s;
// CHECK: @_ZDC2y2dtL_Z1sE1mE = global [2 x i32] zeroinitializer
auto [y2, using s.m] = ar;

int garr[3];
// CHECK: @_ZDC2y3ixL_Z4garrELi1EE = global [2 x i32] zeroinitializer
auto [y3, using garr[1]] = ar;

int &getref();
// CHECK: @_ZDC2y4clL_Z6getrefvEEE = global [2 x i32] zeroinitializer
auto [y4, using getref()] = ar;
