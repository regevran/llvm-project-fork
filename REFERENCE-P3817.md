# P3817 — Implementation Reference

Detailed, implementation-oriented companion to [README-P3817.md](README-P3817.md)
(which has the quick-scan paper-coverage table). This file has the history and
reasoning behind each line of that table: what was fixed, why, and where —
plus the known gaps and repo layout. It is not upstream-quality documentation
and is not meant to be proposed for inclusion in Clang as-is.

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
  into a shared `Sema` member. It was originally called from two places
  (the as-written declaration and, separately, each instantiation), then
  relocated to a single call site inside `CheckCompleteDecompositionDeclaration`
  once packs made that necessary (see below) — that function is already the
  one place both the ordinary and every-instantiation path funnel through
  with a concrete type and a finalized `Bindings` list. Verified this also
  covers a using-marked binding inside a lambda nested in a template with no
  further changes: a lambda's call-operator body is instantiated through the
  same `TemplateDeclInstantiator`/`TreeTransform` machinery as any other
  function body, so there was never a separate path to fix. Covered by
  `clang/test/CodeGenCXX/p3817-using-template.cpp`,
  `clang/test/CodeGenCXX/p3817-using-template-lambda.cpp`,
  `clang/test/SemaCXX/p3817-using-template-constexpr.cpp` (the same
  combined with constant evaluation), and the `from_template` case in
  `clang/test/SemaCXX/p3817-using-illformed.cpp`'s `DuplicateTarget`
  namespace.
- Packs (`using ...expr`) — **Case 1 only**: `expr` must already denote an
  existing pack (e.g. a function parameter pack), not merely contain one as
  a sub-expression (e.g. an index-pack-driven `arr[Is]...` over a single,
  non-pack array — "Case 2", explicitly out of scope for this prototype; an
  issue was filed against the paper for the ambiguity in its own grammar
  this distinction is based on, confirmed with the paper's authors as
  intentionally Case 1 only). Each remaining position in the binding list is
  reused-assigned from the corresponding element of the pack `expr` refers
  to:

  ```cpp
  template <typename... Ts>
  void assign_from(std::tuple<Ts...> src, Ts&... targets) {
      auto [using ...targets] = std::move(src);
  }
  ```

  The grammar needed a genuinely new position: `using ...target` places the
  ellipsis right after `using`, not before it (that leading position is
  reserved for the plain alternative's own, unrelated pack-name marker,
  `auto [x, ...rest] = t;`) — `Parser::ParseDecompositionDeclarator` now
  recognizes it there and wraps the parsed target via `Sema::ActOnPackExpansion`,
  the same function any other pack expansion in the language goes through
  (`f(pack...)`, etc.), so a target that doesn't actually denote a pack is
  rejected by the same generic check, with no bespoke diagnostic needed. The
  two wrong orderings (`...using target`, `using target...`) are both
  rejected without crashing -- the second one is already caught, with a
  correct fix-it, by the plain alternative's own existing misplaced-ellipsis
  recovery, since it runs unconditionally after either alternative parses
  its element.

  The harder part was Sema. `CheckBindingsCount` (which computes how many
  structured-binding positions a pack element accounts for) previously
  assumed *any* pack-typed binding was the plain alternative's, and solved
  for its size from whatever was left over (`MemberCount - Bindings.size() +
  1`) -- silently wrong for a using-pack, whose size is *fixed* by the pack
  it refers to, not solved for. Worse, since nothing checked `UsedDeclaration`
  there, this ran *without any diagnostic*: it built a single, still
  pack-shaped `target = source` assignment instead of one real assignment
  per element, which CodeGen then silently discarded -- confirmed by
  compiling all the way to LLVM IR and finding no store to the caller's
  arguments at all, not merely by reading the AST. Fixed by detecting a
  using-pack distinctly (`isParameterPack() && getReusedTargetExpr()`) and
  handling it separately throughout. Determining *when* a using-pack's size
  is even knowable needed its own new dependency check, independent of the
  existing `DecompType->isDependentType()` one: a using-pack's target can
  still be dependent (we're processing the enclosing template's pattern, not
  one of its instantiations) even when the decomposed type itself isn't
  (e.g. `get()` in the example above returns a concrete type regardless of
  `Ts`) -- `Sema::CheckCompleteDecompositionDeclaration` now checks for this
  and defers completing the whole `DecompositionDecl`, the same way it
  already does for a dependent `DecompType`. This turned out to need no new
  plumbing: `TemplateDeclInstantiator::VisitBindingDecl`'s existing
  substitution of a using-target (the fix described above) already resolves
  a bare pack reference into a `FunctionParmPackExpr` as a side effect, once
  instantiation makes that pack concrete (confirmed via `-ast-dump`, not
  assumed) -- so "is this using-pack resolved yet" is just "is its target's
  pattern still a `DeclRefExpr`, or has it become a `FunctionParmPackExpr`."
  Once resolved, `CheckBindingsCount` builds one concrete, individually
  addressable nested `BindingDecl` per pack element (reusing the plain
  alternative's own `NestedBDs` mechanism), each with its own concrete
  `ReusedTargetExpr` -- and `BuildP3817ReusedAssignments` /
  `CheckP3817DuplicateUsingTargets` were switched from `DD->bindings()`
  (the unflattened, top-level array, still just one entry for the whole
  pack) to `DD->flat_bindings()` (which flattens a *resolved* pack into its
  individual elements), so each element now gets its own real assignment
  and its own duplicate-target check, not one collapsed, inert check for
  the whole pack. Verified with a real execution test (not just IR
  inspection or IR pattern-matching): `f(x, y)` for
  `template<class... Ts> void f(Ts&... targets) { auto [using ...targets] =
  getPair(); }` actually leaves `x`/`y` holding the decomposed values at
  runtime. Covered by `clang/test/CodeGenCXX/p3817-using-pack.cpp` (the
  real-assignment case, Tracker-based like the template tests) and
  `clang/test/SemaCXX/p3817-using-pack-illformed.cpp` (both wrong ellipsis
  positions, a non-pack target, and an arity mismatch against the pack's
  real, expanded size).
- Exercised by the ad hoc programs under `p3817_test/`, and (partially --
  see below) by `clang/test/{SemaCXX,CodeGenCXX}/p3817-*.cpp` lit tests.

## Known gaps

### Paper features not yet implemented

- **Packs** (`using ...expr`) -- **Case 2 only**: an index-pack-driven
  family of targets generated from a single, non-pack object (e.g.
  `arr[Is]...` where `arr` is one array and `Is` is an index pack) is not
  implemented, and is explicitly out of scope for this prototype -- the
  paper's own examples only ever show Case 1 (target already denotes a
  pack), confirmed with the paper's authors (an issue was filed against the
  paper for the underlying grammar ambiguity). Case 1 is implemented -- see
  "What works" above.

### Engineering / process gaps

- **Clang test suite integration: partial.** `clang/test/SemaCXX/p3817-using.cpp`,
  `clang/test/SemaCXX/p3817-using-returned-lvalues.cpp`, and
  `clang/test/CodeGenCXX/p3817-using.cpp` now give `ninja check-clang`
  real `-verify`/`FileCheck` coverage of name resolution, diagnostics, and
  move-vs-copy codegen selection. `clang/test/SemaCXX/p3817-using-illformed.cpp`
  gives real `-verify` coverage for the paper-mandated ill-formed cases
  (duplicate using-targets, `static`/`thread_local`, `using` on the `_`
  placeholder, `const`/`constexpr`, attributes after a using-marked
  element). `clang/test/SemaCXX/p3817-using-pack-illformed.cpp` and
  `clang/test/CodeGenCXX/p3817-using-pack.cpp` give the same kind of
  coverage for using-packs specifically. `clang/test/AST/ast-dump-p3817-using-comma.cpp`
  gives the comma-disambiguation guarantee a grammar-level check too
  (structurally, via `-ast-dump`, that `using a, b` always parses as two
  `BindingDecl`s, regardless of which position is `using`-marked) —
  `p3817_test/` remains the place for things that need real execution
  (values, not just diagnostics/IR shape) — see `comma_disambiguation_test.cpp`
  for why: a structural or `-verify` test for the same guarantee only
  proves the parse *shape* is right, not that the two-way split is
  semantically correct at runtime.
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
- `clang/test/SemaCXX/p3817-*.cpp`, `clang/test/CodeGenCXX/p3817-*.cpp` —
  lit tests (`ninja check-clang`)
- `p3817_test/` — example/scratch programs, and execution-based checks that
  lit's `-verify`/`FileCheck` machinery can't express
