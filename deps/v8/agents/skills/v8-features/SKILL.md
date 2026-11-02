---
name: v8-features
description: Manages the V8 feature shipping lifecycle, including checking feature status and consistency in src/flags/feature-flags.h, advancing features through shipping steps (experimental, pre-stage, stage, ship), checking Finch studies, fieldtrial configs, Origin Trials, and Chromestatus entries, generating Fuzzilli and Linux64 CI test coverage reports for features, bug IDs, CLs, or commits, and preparing V8 launch review documents. Use when the user asks to check, pre-stage, stage, ship, or audit V8 feature flags, check Finch/Origin Trial/Chromestatus status, calculate code coverage for a feature, bug, or commit, or create a V8 launch review. Don't use it for general compiler bug fixing, implementing new language features from scratch, or triaging standalone ClusterFuzz crash dumps.
---

This skill manages V8 features through the V8 shipping lifecycle: checking
feature status, trials, and checklist consistency, advancing features between
shipping steps, generating Fuzzilli and CI test coverage reports, and preparing
launch review documents.

## Quick Reference: `src/flags/feature-flags.h`

All tracked V8 features are defined in `src/flags/feature-flags.h` (which
supersedes legacy pre-`2628639aef5` macros in `src/wasm/wasm-feature-flags.h`
and `JAVASCRIPT_STAGED_FEATURES_BASE` still mentioned in `docs/`):

- **Shipping step macros:**
  - `FOREACH_EXPERIMENTAL_FEATURE_FLAG`: Experimental / in-progress (disabled by
    default)
  - `FOREACH_PRE_STAGED_FEATURE_FLAG`: Pre-staged for fuzzing / VRP (disabled by
    default)
  - `FOREACH_STAGED_FEATURE_FLAG`: Staged behind `--future` (disabled by
    default)
  - `FOREACH_SHIPPED_FEATURE_FLAG`: Shipped (enabled by default)
- **Feature type -> CLI flag mapping:**
  - `WASM_FEATURE(foo_bar, ...)` -> `--wasm-foo-bar` (`v8_flags.wasm_foo_bar`)
  - `JS_FEATURE(foo_bar, ...)` -> `--foo-bar` (`v8_flags.foo_bar`)
  - `INTERNAL_FEATURE(foo_bar, ...)` -> `--foo-bar` (`v8_flags.foo_bar`)

## Parallelization Guidelines

Whenever a task involves multiple features or multiple independent checks on a
single feature, **maximize parallelism across subagents and within-turn tool
calls**:

- **Batch tool calls in every turn:** Issue all independent `read_file`,
  `run_command`, and search calls concurrently in a single turn.
- **Keep parallel subagents read-only:** When sharing a working directory, run
  all repository-mutating steps (editing `src/flags/feature-flags.h`, building
  with `tools/dev/gm.py`, etc.) in the main agent after parallel checks finish.
- **Pass shared context upfront:** Provide each subagent with the feature name,
  CLI flag, macro name, and discovered bug/commit IDs so subagents do not repeat
  discovery work.
- **For coverage reports and launch reviews:** Pre-compute shared coverage
  artifacts once into `/tmp/v8-cov-<feature>/` before fanning out subagents (see
  [references/coverage-report.md](references/coverage-report.md) and
  [references/launch-reviews.md](references/launch-reviews.md)).

## Tasks & Reference Routing

### 1. Determine Status, Check Consistency, or Advance a Step

Load [references/shipping-steps.md](references/shipping-steps.md) when checking
feature status, auditing consistency against `docs/feature-launch-process.md`
and `docs/wasm-shipping-checklist.md`, or advancing a feature to the next step:

- **Status & Trials:** Check the macro and comment block in
  `src/flags/feature-flags.h`, run
  `python3 agents/skills/v8-features/scripts/read_finch_config.py` (which scans
  both google3 Finch `.gcl` studies and Chromium's
  `fieldtrial_testing_config.json`), check Origin Trial registration
  (`src/wasm/wasm-features.cc` or `src/execution/isolate.cc`), and query
  Chromestatus via `fetch_chromestatus.py`.
- **Consistency & Step Advancement:** Verify comment blocks in
  `src/flags/feature-flags.h`, `InitializeGlobal_<id>` in
  `src/init/bootstrapper.cc` (for JS features), `proposal_flags` in
  `test/{wasm-spec-tests,wasm-js}/testcfg.py` (for Pre-staged+ Wasm features),
  and `check_use_counter` in `src/wasm/module-compiler.cc` (enforced by
  `static_assert` for Staged/Shipped Wasm features). Build and verify changes
  with `tools/dev/gm.py quiet x64.optdebug <suite>`.

### 2. Generate a Coverage Report

Load [references/coverage-report.md](references/coverage-report.md) to generate
Fuzzilli (`--source fuzzilli`), V8 Linux64 CI test (`--source tests`), or
combined (`--source both`) line coverage reports for any feature flag, bug ID,
Change-Id, or commit hash via `scripts/generate_coverage_report.py`, and to
triage `**[Needs Triage]**` blocks in parallel.

### 3. Prepare a Launch Review

Load [references/launch-reviews.md](references/launch-reviews.md) and copy the
report skeleton from
[assets/launch-review-template.md](assets/launch-review-template.md). Do not ask
the user for missing information; run Phase 1 pre-fetch, spawn the 8 parallel
research subagents in Phase 2, include checkbox options (`- [ ] ... (tba)`) and
an **Action items** checklist in Phase 3, and save the report to
`/tmp/launch-review-<feature-name>.md` (unless the user specifies another path).
