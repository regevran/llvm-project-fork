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
- Constant evaluation also runs the real assignment, not just runtime
  CodeGen — checked under both constant-evaluation implementations Clang
  ships (`constexpr`/`consteval`/`static_assert`, and again with
  `-fexperimental-new-constant-interpreter`). (Found and fixed along the
  way: CodeGen, the classic `ExprConstant.cpp` evaluator, and the new
  bytecode interpreter each have their own "deferred per-binding init" step
  used for tuple-like `std::get` materialization —
  `CodeGenFunction::MaybeEmitDeferredVarDeclInit`,
  `EvaluateDecompositionDeclInit`, and
  `Compiler<Emitter>::maybeEmitDeferredVarInit` respectively — and only the
  CodeGen one had originally been taught about `ReusedAssignment`. A
  using-marked binding evaluated in a constant expression silently kept the
  target's old value instead of running the assignment or failing to
  compile.)
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
  rejected; four are now fixed, one is intentionally deferred (verified —
  no diagnostic, no crash, just wrong behavior):
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
  - ~~`using` on the `_` placeholder~~ — **fixed.** Rejected at the parser
    level, before it ever reaches ordinary expression Sema: leaving it to
    expression lookup would silently accept `using _` whenever exactly one
    placeholder happens to be in scope (Sema only diagnoses a *reference*
    to `_` when it's ambiguous between multiple placeholders, not when
    there's a lone one to resolve to unambiguously).
  - **`const` on the structured binding itself with `using`-elements:
    intentionally not implemented.** The paper mandates this as
    ill-formed (only "target is const" is currently checked, which is a
    different, narrower rule), but that's the paper authors' own judgment
    call (see "Alternative Considered" under `#const` in `P3817.md`), not
    a language constraint, and it's the kind of decision EWGI/EWG
    discussion is likely to revisit. Deferred rather than implemented
    against a rule expected to change.
  - ~~Attributes after a `using`-marked element~~ — **fixed.** No
    attribute-specifier-seq appears in the using-marked alternative of
    sb-identifier — attributes appertain to a newly declared variable, and
    a using-marked element introduces none. (Unlike `const`, there's no
    known EWG sentiment either way on this one; implemented anyway since
    the fix reuses `Parser::DiagnoseAndSkipCXX11Attributes()`, already
    used elsewhere in the same function for the same purpose.)

### Engineering / process gaps

- **Clang test suite integration: partial.** `clang/test/SemaCXX/p3817-using.cpp`,
  `clang/test/SemaCXX/p3817-using-returned-lvalues.cpp`, and
  `clang/test/CodeGenCXX/p3817-using.cpp` now give `ninja check-clang`
  real `-verify`/`FileCheck` coverage of name resolution, diagnostics, and
  move-vs-copy codegen selection. `clang/test/SemaCXX/p3817-using-illformed.cpp`
  now also gives real `-verify` coverage for the four fixed ill-formed cases
  above. Still missing: a Parser-level test for the comma-disambiguation
  guarantee at the grammar level. `p3817_test/`
  remains the place for things that need
  real execution (values, not just diagnostics/IR shape) — see
  `comma_disambiguation_test.cpp` for why: the SemaCXX test for the same
  guarantee only proves absence of one failure signature (an arity
  mismatch), not that the two-way split is semantically correct.
- ~~AST serialization (PCH/modules) untouched.~~ — **fixed.**
  `BindingDecl::ReusedTargetExpr` and `ReusedAssignment` are now written and
  read back by `ASTDeclWriter`/`ASTDeclReader::VisitBindingDecl`, the same
  way the pre-existing `Binding` field always was. Verified end-to-end by
  hand (not just re-reading the two fields) both ways the paper's gap
  description names: a PCH boundary and a C++20 named-module boundary, in
  each case by having the assignment actually run *after* the round trip
  and observing the target's new value — since it's easy to write a
  serialization fix that reads back non-null-but-wrong exprs and still pass
  a naive test. `clang/test/PCH/p3817-using.cpp` gives this regression
  coverage (the module boundary isn't separately covered — no existing test
  in this tree covers *any* decomposition declaration across a module
  boundary, using-marked or not, and both boundaries share the exact same
  `ASTWriter`/`ASTReader` code this fix touches).
- ~~No `-ast-dump` support.~~ — **fixed.** `TextNodeDumper::VisitBindingDecl`
  now appends a ` using` marker to a using-marked binding's header line, and
  `ASTNodeTraverser::VisitBindingDecl` dumps its target and the built
  assignment as children — instead of `getBinding()`, which is already
  reachable as the assignment's source operand, so dumping both would show
  the same subexpression twice. This shared traversal is also what
  `-ast-dump=json` walks, so JSON dumps pick up the same two children with
  no separate change. Covered by `clang/test/AST/ast-dump-p3817-using.cpp`.
- **`-ast-print` still open — being looked at separately.** `DeclPrinter`
  has no decomposition-declaration support at all yet, `using`-marked or
  not: `auto [x, y] = get();` currently prints as `auto = get();`, silently
  dropping the whole `[x, y]` pattern. Fixing the `using`-marked case means
  fixing that pre-existing gap too, so it's more than the "teach one more
  visitor about two fields" shape the other three fixes here had.
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
