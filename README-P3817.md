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
- Exercised by the ad hoc programs under `p3817_test/`.

## Known gaps

### Paper features not yet implemented

- **Templates.** The using-target is resolved once, at initial parse of the
  template; nothing re-derives it at instantiation. `using` inside a
  template body currently compiles clean but silently does nothing —
  no diagnostic.
- **Packs** (`using ...expr`). Not implemented; deferred alongside
  templates.
- Several paper-mandated **ill-formed cases are currently silently
  accepted** instead of rejected (verified — no diagnostic, no crash, just
  wrong behavior):
  - Duplicate using-targets in one binding list: `auto [using x, using x] = ...;`
  - `static`/`thread_local` combined with `using`
  - `using` on the `_` placeholder
  - `const` on the structured binding itself when it has `using`-elements,
    independent of whether the target itself is const (only "target is
    const" is currently checked, which is a different, narrower rule)
  - Attributes after a `using`-marked element (the paper disallows these)

### Engineering / process gaps

- **No clang test suite integration.** Everything lives in `p3817_test/` as
  plain `.cpp` programs, not `clang/test/{Parser,SemaCXX,CodeGenCXX}` lit
  tests with `-verify`/`-ast-dump`/`FileCheck` annotations. `ninja
  check-clang` exercises none of this feature.
- **AST serialization (PCH/modules) untouched.** `BindingDecl::ReusedDecl`,
  `ReusedTargetExpr`, and `ReusedAssignment` are never read or written by
  `ASTReader`/`ASTWriter`. A `using`-marked binding compiled through a PCH
  or C++20 module boundary will currently lose them silently — the same
  failure mode as the template gap above, via a different boundary.
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
- `p3817_test/` — example/scratch programs
