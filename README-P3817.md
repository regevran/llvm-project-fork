# P3817 — Structured Binding Assignments (Clang prototype)

This branch is an experimental, work-in-progress implementation of
[P3817R1](P3817.md) — an extension letting a structured binding assign to a
pre-existing variable via the `using` keyword:

```cpp
int id;
auto [using id, name] = get_record();  // id is assigned; name is declared
```

This file tracks implementation status at a glance. It is not
upstream-quality documentation and is not meant to be proposed for
inclusion in Clang as-is. For the history and reasoning behind each line
below — what was fixed, why, and where — see
[REFERENCE-P3817.md](REFERENCE-P3817.md).

## Paper coverage at a glance

| Paper section | Status |
| --- | --- |
| [Syntax](P3817.md#syntax) (`using` before a binding-list element) | ✅ Implemented |
| [Semantics](P3817.md#semantics) (real `operator=`, move/copy per ref-qualifier, left-to-right assignment order) | ✅ Implemented — all 3 decomposition kinds, local + global scope, constant evaluation |
| [`const`](P3817.md#const) / [`constexpr`](P3817.md#constexpr) | ⚠️ Ill-formed by default (matches the paper); opt-outable via `-fstructured-binding-assignment-allow-const` |
| [Storage Class](P3817.md#storage-class) (`static`/`thread_local`) | ✅ Implemented (ill-formed) |
| [`constinit`](P3817.md#constinit) | ✅ Implemented (ill-formed, follows from Storage Class) |
| [Returned Lvalues](P3817.md#returned-lvalues) (`using foo()`, `using s[0]`, `using obj.member`) | ✅ Implemented |
| [C++26 `_` Placeholder](P3817.md#c26-_-placeholder) | ✅ Implemented (ill-formed) |
| [Duplicate Variables](P3817.md#duplicate-variables-ill-formed-for-assigned-elements) | ✅ Implemented (ill-formed) |
| [Packs](P3817.md#packs) (`using ...expr`) | ⚠️ Partially implemented — only when `expr` already denotes a pack ("Case 1"); an index-pack-driven family of targets from a single non-pack object ("Case 2") is explicitly out of scope, pending the paper's own authors |
| Templates (re-derivation at instantiation) | ✅ Implemented — target re-derived per instantiation, including inside a lambda nested in a template and inside a pack; duplicate-target check re-run per instantiation |

## Clang implementation coverage at a glance

Separate from the paper itself — the engineering bar Clang expects of any
language extension, prototype or not.

| Item | Status |
| --- | --- |
| Experimental-extension gating (`-fstructured-binding-assignment`) | ✅ Implemented — `using` rejected without the flag, in every `-std=` mode |
| `-ast-dump` support | ✅ Implemented |
| `-ast-print` support | ✅ Implemented |
| AST serialization (PCH / C++20 modules) | ✅ Implemented — PCH has direct regression coverage; the module boundary shares the same reader/writer code but has no P3817-specific test of its own |
| Test suite integration (`ninja check-clang`) | ⚠️ Partial — see REFERENCE-P3817.md's "Known gaps" |
| Documentation (`ReleaseNotes.rst`, `docs/LanguageExtensions.rst`, `clang/www/cxx_status.html`) | ❌ Not started |

Not upstream-tracked: the reasoning behind each line of both tables is in
REFERENCE-P3817.md's "What works"/"Known gaps" section.
