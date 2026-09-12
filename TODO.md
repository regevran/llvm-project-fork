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

- Templates: a `using`-marked binding inside a (non-lambda) template
  function body is now correctly re-derived at each instantiation --
  `TemplateDeclInstantiator::VisitBindingDecl` used to clone a `BindingDecl`
  without its P3817 target expression at all, so `using` silently did
  nothing post-instantiation. Fixed in two steps: propagating the
  (substituted) target expression itself, and re-running the
  duplicate-using-target check per instantiation (two using-targets that
  are distinct as written, e.g. `using arr[I], using arr[J]`, can still
  collide once a specific instantiation's arguments are known -- the
  as-written check can't see that). Verified this also covers a
  using-marked binding inside a lambda nested in a template with no
  further changes -- a lambda's call operator body is instantiated
  through the same TemplateDeclInstantiator/TreeTransform machinery, so
  there was never a separate path to fix.
- Packs (`using ...expr`) — not implemented, deferred alongside templates.

### Test/process gaps

- No documentation (`ReleaseNotes.rst`, `docs/LanguageExtensions.rst`,
  `clang/www/cxx_status.html`) — not a blocker, since this branch isn't
  intended to land upstream as-is.
