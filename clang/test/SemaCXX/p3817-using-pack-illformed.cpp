// RUN: %clang_cc1 -std=c++2c -fstructured-binding-assignment -fsyntax-only -verify %s

// P3817: ill-formed cases specific to using-packs (`using ...expr`, "Case
// 1" -- expr must already denote a pack; see REFERENCE-P3817.md's "Packs"
// entries for "Case 2" -- an index-pack-driven family of targets from a
// single non-pack object -- which is explicitly out of scope for this
// prototype).

struct Pair { int a, b; };
Pair get();

namespace WrongEllipsisPosition {
// The ellipsis belongs right after 'using', not before it -- that leading
// position is reserved for the plain alternative's own (unrelated)
// pack-name marker, so a leading ellipsis immediately followed by 'using'
// is left unconsumed and just fails to parse as anything valid.
template <class... Ts> void leading(Ts&... targets) {
  auto [a, ...using targets] = get();
  // expected-error@-1 {{expected identifier}}
  // expected-error@-2 {{type 'Pair' binds to 2 elements, but only 1 name was provided}}
}

// Nor after the target expression -- that's recovered (with a FixIt) by
// the same diagnostic the plain alternative's own misplaced-ellipsis
// mistake already gets.
template <class... Ts> void trailing(Ts&... targets) {
  auto [using targets...] = get();
  // expected-error@-1 {{'...' must immediately precede declared identifier}}
}
} // namespace WrongEllipsisPosition

namespace NotAPack {
// 'using ...x' requires x to already denote a pack -- rejected by the same
// check every other pack expansion in the language gets.
template <class T> void f(T& x) {
  auto [using ...x] = get();
  // expected-error@-1 {{pack expansion does not contain any unexpanded parameter packs}}
  // expected-warning@-2 {{ISO C++17 does not allow a structured binding group to be empty}}
  // expected-error@-3 {{type 'Pair' binds to 2 elements, but no names were provided}}
}
} // namespace NotAPack

namespace Arity {
// A using-pack's own size is fixed by the pack it refers to, not solved
// for from the decomposition's member count the way the plain
// alternative's pack is -- a mismatch is diagnosed with the real, expanded
// element count (3, here), not the pre-expansion "1" binding this list
// actually has as written.
template <class... Ts> void too_many(Ts&... targets) {
  auto [using ...targets] = get();
  // expected-error@-1 {{type 'Pair' binds to 2 elements, but 3 names were provided}}
}
template void too_many<int, int, int>(int&, int&, int&);
// expected-note@-1 {{in instantiation of function template specialization 'Arity::too_many<int, int, int>' requested here}}
} // namespace Arity
