# Open items — `p3817` branch and upstream PR #221711

Working list, not a permanent doc. Merges what's open on the experimental
`p3817` branch (this repo) with what's open on the real upstream PR
[llvm/llvm-project#221711](https://github.com/llvm/llvm-project/pull/221711)
(branch `ast-print-decomposition-decl`).

## Upstream PR #221711 (`-ast-print` support for `DecompositionDecl`)

### CI is currently red

Linux, Windows, and macOS-arm64 CI jobs fail. Root cause identified: the fix
correctly changes how a `DecompositionDecl` prints (from dropping `[a, b]`
entirely to showing it), which breaks two **pre-existing** tests whose
`CHECK` lines were pinned to the old, buggy output:

- `clang/test/Analysis/cfg.cpp:658` —
  `CHECK-NEXT: 8: auto = {{\{}}arr[*]{{(\})}};` needs updating; it now
  prints `auto [a, b] = ...`.
- `clang/test/Analysis/anonymous-decls.cpp:75` —
  `CHECK-NEXT: 5: auto &;` needs updating similarly.
- (The Linux-AArch64 job's failures are the same two tests.)

Action: update both tests' expected output to match the corrected printing,
as part of this PR.

### Unaddressed review comments (AaronBallman, 2026-09-08)

1. `clang/lib/AST/DeclPrinter.cpp:471` — combine the `isa<ObjCIvarDecl>(*D)`
   and `isa<BindingDecl>(*D)` checks into one
   `isa<ObjCIvarDecl, BindingDecl>(*D)` and merge the two comments.
   Cosmetic, easy.
2. `clang/test/AST/ast-print-decomposition.cpp:1` — "Does something about
   the test require C++26?" Yes: the `Packs` namespace exercises the C++26
   structured-binding-pack extension (`auto [first, ...rest, last] = arr;`);
   nothing else in the file needs it. Action: either add a comment
   explaining why, or split the pack case into its own `-std=c++26` file so
   the rest can stay at `-std=c++20`.
3. `clang/test/AST/ast-print-decomposition.cpp:34` — "Where is the
   initializer?" `CHECK-NEXT: auto [a, b, c]` for the array case never
   checks that `= arr;` is actually printed — a real test-rigor gap.
   Action: extend to `CHECK-NEXT: auto [a, b, c] = arr;`.
4. `clang/test/AST/ast-print-decomposition.cpp:74` — same gap for the pack
   case: extend to
   `CHECK-NEXT: auto [first, ...rest, last] = arr;`.
5. `clang/test/AST/ast-print-decomposition.cpp:86` — "Why is this using a
   regex for the attribute?" Because `[[` is FileCheck's own
   variable-reference syntax, so a literal `[[maybe_unused]]` must be
   escaped as `{{\[\[}}maybe_unused{{\]\]}}`. Action: reply explaining this
   (no code change needed) unless a cleaner alternative turns up.

## `p3817` branch — engineering gaps

### Bugs found, not yet fixed

- **`failed_p3817_program.cpp`** (repo root, untracked) — the original
  Compiler Explorer crash report. The specific crash it showed is fixed;
  the file itself hasn't been revisited to confirm nothing else in it is
  still open.

### Not yet implemented (paper features)

- Templates: `using` inside a template body silently does nothing at
  instantiation (no re-derivation, no diagnostic).
- Packs (`using ...expr`) — not implemented, deferred alongside templates.
- `const`/`constexpr` on a using-containing structured binding —
  intentionally deferred; the paper authors themselves flag this as
  unsettled EWG/EWGI territory, not a hard language constraint.

### Test/process gaps

- No Parser-level test for the comma-disambiguation guarantee at the
  grammar level (only covered today by the execution-based
  `p3817_test/comma_disambiguation_test.cpp`).
- No `clang/test/Parse` (or SemaCXX) lit coverage yet for the
  unary-expression using-target fix (`1678e1bef254`,
  `using ++x` / `using *p`) — only verified via scratch files so far.
- No documentation (`ReleaseNotes.rst`, `docs/LanguageExtensions.rst`,
  `clang/www/cxx_status.html`) — not a blocker, since this branch isn't
  intended to land upstream as-is.

### Housekeeping

- Propagate the two `p3817`-only fixes (mangling crash `69555cf8481f`,
  parser-disambiguation fix `1678e1bef254`) into the PR branch, or into
  `p3817`'s eventual squashed history, at the next deliberate sync
  checkpoint (per the established two-branch workflow — not urgent by
  itself).
