# P3817 — Structured Binding Assignments (Clang prototype)

This branch is an experimental, work-in-progress implementation of
[P3817R0](P3817.md) — an extension letting a structured binding assign to a
pre-existing variable via the `using` keyword:

```cpp
int id;
auto [using id, name] = get_record();  // id is assigned; name is declared
```

This file tracks implementation status. It is not upstream-quality
documentation and is not meant to be proposed for inclusion in Clang as-is.

## What works

- Parsing `using <unary-expression>` before an element in the
  structured-binding identifier list — a bare identifier (`using x`) is
  just the simplest case; `using foo()`, `using s[0]`, `using m["k"]`,
  `using obj.member` are all accepted too ("Returned Lvalues" from the
  paper's "Further Design Decisions").
- The target expression is parsed and resolved via Clang's ordinary
  expression grammar/Sema (`Parser::ParseCastExpression` with
  `CastParseKind::UnaryExprOnly`), so name lookup, implicit member access
  (`this->`), and diagnostics (undeclared name, misuse from a static member
  function) all come from the same machinery any other expression goes
  through — no bespoke lookup code needed. Assigning to a const-qualified
  target is rejected the same way an ordinary `x = ...;` assignment would
  be, via `Sema::BuildBinOp`, once the real assignment is built.
- CodeGen emits the real assignment through the actual `operator=` (not a
  raw store), with move-vs-copy selection following the paper's
  ref-qualifier rule (`auto` → move, `auto&`/`auto&&` bound to an lvalue →
  copy), across all three decomposition kinds: array/vector/complex,
  tuple-like (via `std::get`, e.g. `std::pair`/`std::tuple`), and
  non-tuple-like class member decomposition.
- Exercised by the ad hoc programs under `p3817_test/`, and (partially --
  see below) by `clang/test/{SemaCXX,CodeGenCXX}/p3817-*.cpp` lit tests.

## Known gaps

### Paper features not yet implemented

- **Templates.** The using-target is resolved once, at initial parse of the
  template; nothing re-derives it at instantiation. `using` inside a
  template body currently compiles clean but silently does nothing —
  no diagnostic.
- **Packs** (`using ...expr`). Not implemented; deferred alongside
  templates.
- Several paper-mandated ill-formed cases were silently accepted instead of
  rejected; two are now fixed, the rest remain (verified — no diagnostic,
  no crash, just wrong behavior):
  - ~~Duplicate using-targets in one binding list~~ — **fixed.**
    `auto [using x, using x] = ...;` is now rejected, along with the same
    variable reached through a member-access or constant-index-subscript
    chain (`using s.m` / `using arr[0]` repeated). Detection is
    intentionally conservative/structural (same idea as the existing
    self-comparison-warning helper `Expr::isSameComparisonOperand`, entered
    directly since using-targets are unconverted lvalues): two using-targets
    that are function calls (`using foo()`) are never flagged, since two
    calls need not return the same lvalue.
  - ~~`static`/`thread_local` combined with `using`~~ — **fixed.** Unlike
    ordinary structured bindings (where C++20 permits these), the
    combination is now always ill-formed when the binding list has a
    using-marked element.
  - `using` on the `_` placeholder
  - `const` on the structured binding itself when it has `using`-elements,
    independent of whether the target itself is const (only "target is
    const" is currently checked, which is a different, narrower rule)
  - Attributes after a `using`-marked element (the paper disallows these)

### Engineering / process gaps

- **Clang test suite integration: partial.** `clang/test/SemaCXX/p3817-using.cpp`,
  `clang/test/SemaCXX/p3817-using-returned-lvalues.cpp`, and
  `clang/test/CodeGenCXX/p3817-using.cpp` now give `ninja check-clang`
  real `-verify`/`FileCheck` coverage of name resolution, diagnostics, and
  move-vs-copy codegen selection. `clang/test/SemaCXX/p3817-using-illformed.cpp`
  now also gives real `-verify` coverage for the two fixed ill-formed cases
  above. Still missing: a Parser-level test for the comma-disambiguation
  guarantee at the grammar level, and gap-documentation tests for the
  remaining ill-formed-but-currently-accepted cases listed above so a
  future fix has something to flip to "expected-error". `p3817_test/`
  remains the place for things that need
  real execution (values, not just diagnostics/IR shape) — see
  `comma_disambiguation_test.cpp` for why: the SemaCXX test for the same
  guarantee only proves absence of one failure signature (an arity
  mismatch), not that the two-way split is semantically correct.
- **AST serialization (PCH/modules) untouched.** `BindingDecl::ReusedTargetExpr`
  and `ReusedAssignment` are never read or written by `ASTReader`/`ASTWriter`.
  A `using`-marked binding compiled through a PCH or C++20 module boundary
  will currently lose them silently — the same failure mode as the
  template gap above, via a different boundary.
- **No `-ast-dump`/`-ast-print` support.** `ASTDumper.cpp`,
  `DeclPrinter.cpp`, and `TextNodeDumper.cpp` have no awareness of
  `using`-marked bindings.
- **No experimental-extension gating or warning.** Compiles unconditionally
  in any `-std=` mode, with no flag to opt in/out and no diagnostic
  flagging usage as a non-standard, not-yet-adopted extension.
- **No documentation.** No `ReleaseNotes.rst` entry, no
  `docs/LanguageExtensions.rst` mention, no `clang/www/cxx_status.html`
  entry.

## Layout

- `P3817.md` — the paper text (P3817R0)
- `clang/include/clang/Sema/DeclSpec.h`, `clang/lib/Parse/ParseDecl.cpp` —
  grammar
- `clang/include/clang/AST/DeclCXX.h` — `BindingDecl` data model
- `clang/lib/Sema/SemaDeclCXX.cpp` — name resolution and assignment
  building
- `clang/lib/CodeGen/CGDecl.cpp` — codegen
- `clang/test/SemaCXX/p3817-*.cpp`, `clang/test/CodeGenCXX/p3817-using.cpp` —
  lit tests (`ninja check-clang`)
- `p3817_test/` — example/scratch programs, and execution-based checks that
  lit's `-verify`/`FileCheck` machinery can't express
