# Open items — `p3817` branch and its spun-off upstream fixes

Working list, not a permanent doc. `p3817` is rebased on top of current
`upstream/main`, which already includes
[llvm/llvm-project#221711](https://github.com/llvm/llvm-project/pull/221711)
(`-ast-print` support for `DecompositionDecl`) — that PR is merged and fully
closed out, no open items remain there.

## Spun-off upstream PRs

- [llvm/llvm-project#222881](https://github.com/llvm/llvm-project/pull/222881)
  — fixes the `msvc-link.c` test flake found via CI noise on #221711 (see
  [#222872](https://github.com/llvm/llvm-project/issues/222872)): a bare
  `DEBUG-LINK-NOT: lld` collided with lit's randomly-generated scratch
  directory name. Open, awaiting review/merge.

## `p3817` branch — engineering gaps

### Not yet implemented (paper features)

- Templates: `using` inside a template body silently does nothing at
  instantiation (no re-derivation, no diagnostic).
- Packs (`using ...expr`) — not implemented, deferred alongside templates.

### Test/process gaps

- P3817R1 added a normative left-to-right assignment order guarantee
  across all three decomposition kinds (see the "Semantics" section of
  `P3817.md`). Verified correct by inspection, but no dedicated lit test
  enforces it yet — a future change could silently break it undetected.
- No documentation (`ReleaseNotes.rst`, `docs/LanguageExtensions.rst`,
  `clang/www/cxx_status.html`) — not a blocker, since this branch isn't
  intended to land upstream as-is.
