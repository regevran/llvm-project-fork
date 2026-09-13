# P3817 — Structured Binding Assignments (Clang prototype)

This branch is an experimental, work-in-progress implementation of
[P3817R1](P3817.md) — an extension letting a structured binding assign to a
pre-existing variable via the `using` keyword:

```cpp
int id;
auto [using id, name] = get_record();  // id is assigned; name is declared
```

This file tracks implementation status. It is not upstream-quality
documentation and is not meant to be proposed for inclusion in Clang as-is.

## Paper coverage at a glance

A quick-scan complement to the narrative sections below, mapped directly to
the paper's own section structure — see "What works"/"Known gaps" for the
history and reasoning behind each status.

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
| [Packs](P3817.md#packs) (`using ...expr`) | ❌ Not implemented |
| Templates (re-derivation at instantiation) | ✅ Implemented — target re-derived per instantiation, including inside a lambda nested in a template; duplicate-target check re-run per instantiation |

## What works

- Parsing `using <unary-expression>` before an element in the
  structured-binding identifier list — a bare identifier (`using x`) is
  just the simplest case; `using foo()`, `using s[0]`, `using m["k"]`,
  `using obj.member` are all accepted too ("Returned Lvalues" from the
  paper's "Further Design Decisions"), and so is a prefix unary operator
  (`using ++x`, `using *p`) — `Parser::ParseDecompositionDeclarator`'s
  structured-binding-list-vs-misplaced-array-declarator disambiguation
  heuristic used to reject these outright, before Sema ever got a chance
  to accept or reject them on their merits; fixed by recognizing that
  `using` is a keyword, so this disambiguation never even applies to it in
  the first place (unlike a bare identifier or an ellipsis, which really
  can start an array-bound expression, and do need the lookahead). A
  prefix operator that doesn't yield an lvalue (`using -x`, `using &x`)
  still correctly fails afterward, at Sema's ordinary assignability check
  — the fix is only about the parser no longer pre-empting that. Covered
  by `clang/test/SemaCXX/p3817-using-unary-target.cpp`.
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
- A namespace-scope (global) using-marked decomposition no longer crashes
  the compiler, and its assignment now actually runs. Clang mangles the
  hidden decomposed object's linkage name as `DC<source-name>*E`, built
  from each binding's name (`ItaniumMangle.cpp`) — but a using-marked
  binding has no name of its own (`Id` is null), so `mangleSourceName`
  dereferenced a null `IdentifierInfo*` and segfaulted for anything beyond
  a purely local binding (the only kind exercised before this was found).
  Fixed by mangling the using-target's own expression instead of a
  source-name for that binding, reusing Clang's existing general-purpose
  expression mangler (`CXXNameMangler::mangleExpression`) — already
  covers every kind of using-target the grammar allows (a plain
  reference, a member access, a subscript, a call). Also fixes the ODR
  concern a naive "skip it" fix would have had: two decompositions that
  are entirely using-marked, e.g. two `auto [using a, using b] = ...;`
  with different targets, would otherwise both mangle to the identical
  bare `DCE`.

  That fix only stopped the crash — the assignment itself still silently
  didn't run: CodeGen's local-declaration path
  (`CodeGenFunction::MaybeEmitDeferredVarDeclInit`) was P3817-aware, but
  the global-variable-definition path never learned about
  `ReusedAssignment` at all. Fixing this for the aggregate/array
  decomposition kinds (no per-binding holding var) was direct — emit each
  binding's reused assignment right after the `DecompositionDecl`'s own
  init, in its ctor (`CodeGenFunction::EmitCXXGlobalVarDeclInit`). The
  tuple-like kind (the one with a holding var per binding) needed more
  care: a using-marked binding's holding var has to be initialized inside
  that *same* ctor, immediately before the assignment reading through it
  — two separate top-level ctors only guarantee relative order via append
  order into `llvm.global_ctors`, which isn't good enough here in either
  direction (the holding var needs the decomposition's own storage
  already set, and the assignment needs the holding var already set). So
  `CodeGenModule::EmitTopLevelDecl` now deliberately skips emitting such a
  holding var as its own top-level global, and it's given proper
  definition status and initialized inline instead. That holding var also
  turned out to need a name: it's unnamed for the same reason a
  using-marked binding itself is, but Itanium mangling has no scheme for
  an unnamed *ordinary* `VarDecl` (only for anonymous unions/structs) —
  `mangleUnqualifiedName`'s unconditional `getType()->castAsRecordDecl()`
  crashed. Fixed by synthesizing a name only when one is actually needed
  (global storage duration) and giving it internal linkage, since nothing
  outside this one translation unit could ever reference it. Covered by
  `clang/test/CodeGenCXX/p3817-using-global.cpp`, including the mixed
  marked/unmarked-binding and reopened-namespace cases that most directly
  exercise this ordering.
- A using-marked binding inside a template body now correctly re-derives its
  target and assignment at each instantiation, instead of silently doing
  nothing. `TemplateDeclInstantiator::VisitBindingDecl` used to clone a
  `BindingDecl` without its P3817 target expression at all; fixed by
  substituting the target expression the same way any other pattern
  expression is substituted (`SemaRef.SubstExpr`) — the reused *assignment*
  itself isn't cloned, it's rebuilt from scratch once the instantiated
  `DecompositionDecl` is completed, the same way it is for a non-template
  decomposition. Two using-targets that are distinct as written (e.g.
  `using arr[I], using arr[J]`) can still collide once a specific
  instantiation's arguments are substituted in — the as-written duplicate
  check can't see that, since it only ever runs once, on the unsubstituted
  pattern — so the check (`CheckP3817DuplicateUsingTargets`) was pulled out
  into a shared `Sema` member and re-run per instantiation too. Verified
  this also covers a using-marked binding inside a lambda nested in a
  template with no further changes: a lambda's call-operator body is
  instantiated through the same `TemplateDeclInstantiator`/`TreeTransform`
  machinery as any other function body, so there was never a separate path
  to fix. Covered by `clang/test/CodeGenCXX/p3817-using-template.cpp`,
  `clang/test/CodeGenCXX/p3817-using-template-lambda.cpp`, and the
  `from_template` case in `clang/test/SemaCXX/p3817-using-illformed.cpp`'s
  `DuplicateTarget` namespace.
- Exercised by the ad hoc programs under `p3817_test/`, and (partially --
  see below) by `clang/test/{SemaCXX,CodeGenCXX}/p3817-*.cpp` lit tests.

## Known gaps

### Paper features not yet implemented

- **Packs** (`using ...expr`). Not implemented.

### Engineering / process gaps

- **Clang test suite integration: partial.** `clang/test/SemaCXX/p3817-using.cpp`,
  `clang/test/SemaCXX/p3817-using-returned-lvalues.cpp`, and
  `clang/test/CodeGenCXX/p3817-using.cpp` now give `ninja check-clang`
  real `-verify`/`FileCheck` coverage of name resolution, diagnostics, and
  move-vs-copy codegen selection. `clang/test/SemaCXX/p3817-using-illformed.cpp`
  gives real `-verify` coverage for the paper-mandated ill-formed cases
  (duplicate using-targets, `static`/`thread_local`, `using` on the `_`
  placeholder, `const`/`constexpr`, attributes after a using-marked
  element). `clang/test/AST/ast-dump-p3817-using-comma.cpp` gives the
  comma-disambiguation guarantee a grammar-level check too (structurally,
  via `-ast-dump`, that `using a, b` always parses as two `BindingDecl`s,
  regardless of which position is `using`-marked) — `p3817_test/`
  remains the place for things that need real execution (values, not just
  diagnostics/IR shape) — see `comma_disambiguation_test.cpp` for why: a
  structural or `-verify` test for the same guarantee only proves the
  parse *shape* is right, not that the two-way split is semantically
  correct at runtime.
- **No documentation.** No `ReleaseNotes.rst` entry, no
  `docs/LanguageExtensions.rst` mention, no `clang/www/cxx_status.html`
  entry.

## Layout

- `P3817.md` — the paper text (P3817R1)
- `clang/include/clang/Sema/DeclSpec.h`, `clang/lib/Parse/ParseDecl.cpp` —
  grammar
- `clang/include/clang/AST/DeclCXX.h` — `BindingDecl` data model
- `clang/lib/Sema/SemaDeclCXX.cpp` — name resolution and assignment
  building
- `clang/lib/Sema/SemaTemplateInstantiateDecl.cpp` — re-deriving a
  using-target at template instantiation
- `clang/lib/CodeGen/CGDecl.cpp` — codegen
- `clang/test/SemaCXX/p3817-*.cpp`, `clang/test/CodeGenCXX/p3817-using.cpp` —
  lit tests (`ninja check-clang`)
- `p3817_test/` — example/scratch programs, and execution-based checks that
  lit's `-verify`/`FileCheck` machinery can't express
