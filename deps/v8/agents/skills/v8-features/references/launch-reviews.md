# V8 Launch Review Workflow

Prepare a comprehensive V8 launch review document by running a 3-phase
multi-agent workflow and populating
[assets/launch-review-template.md](../assets/launch-review-template.md).

## Workflow Guidelines

1. **Do not ask the user:** Complete all local codebase, script, and git history
   research and generate the full report directly without prompting the user for
   missing information.
2. **Parallelize with subagents:** After a quick Phase 1 discovery & coverage
   pre-fetch on the main agent, spawn parallel read-only subagents in a single
   turn to research all sections concurrently.
3. **Include options in the report:** When information cannot be inferred
   automatically (e.g., design docs, rollout timelines, trial plans, known
   risks, partner outreach, or cross-component reviews), keep the checkbox
   options (`- [ ] ... (tba)`) from
   [assets/launch-review-template.md](../assets/launch-review-template.md) so
   the user can select or fill them in.
4. **Create a list of action items:** Conclude with an **Action items**
   checklist (`- [ ] ...`) covering both `tba` user decisions and any
   engineering or process gaps discovered during the review.
5. **Use relative repository paths:** Always format V8 file references relative
   to the V8 repository root (never absolute filesystem paths or leading
   slashes).
6. **Save outside the git working tree by default:** Write the synthesized
   document to `/tmp/launch-review-<feature-name>.md` (or the agent's
   artifact/scratch directory), unless the user explicitly specifies an output
   path. Never leave untracked files in the V8 repository root.

______________________________________________________________________

## Phase 1: Initial Discovery & Coverage Pre-Fetch (Main Agent)

Before spawning subagents, establish shared context and pre-compute both
coverage reports in a single pass so no subagent repeats git history,
`git blame`, or coverage downloads:

1. **Locate the feature flag & macro (`src/flags/feature-flags.h`):**
   - Identify the macro
     (`FOREACH_{EXPERIMENTAL,PRE_STAGED,STAGED,SHIPPED}_FEATURE_FLAG`), feature
     type (`WASM_FEATURE`, `JS_FEATURE`, `INTERNAL_FEATURE`), CLI flag name, and
     preceding comment block (or check `src/flags/flag-definitions.h` if not yet
     in `src/flags/feature-flags.h`; see
     [references/shipping-steps.md](shipping-steps.md)).
   - If only a bug ID is provided, run
     `git log -E -i --grep="^(Bug|Fixed):.*<bug_id>" --oneline -n 20` first to
     identify the feature flag name and commit subject prefix (e.g.,
     `[wasm-foo]`).
2. **Inspect flag transition history & pre-fetch both coverage reports in
   parallel:**
   - Run `git log -G` across current and historical flag files (prefer `-G` over
     `-S` or `git blame`, which misses same-file macro moves and breaks on bulk
     refactor `2628639aef5`):
     ```bash
     git log -G"\b<feature_name>\b" -p -- src/flags/feature-flags.h src/wasm/wasm-feature-flags.h src/flags/flag-definitions.h
     ```
   - Concurrently run `generate_coverage_report.py` with `--source both` and
     `--output-dir /tmp/v8-cov-<feature_name>` (see
     [references/coverage-report.md](coverage-report.md)):
     ```bash
     python3 agents/skills/v8-features/scripts/generate_coverage_report.py \
       --source both \
       --output-dir /tmp/v8-cov-<feature_name> \
       --feature <feature_name> [<bug_id> ...] [--grep "<subject_prefix>"]
     ```
     This writes
     `/tmp/v8-cov-<feature_name>/{commits.txt,bugs.txt,fuzzilli-coverage.md,tests-coverage.md}`.
     Reuse `commits.txt` and `bugs.txt` across all downstream steps (do **not**
     re-run `find_affected_blocks.py --hashes-only` or
     `generate_coverage_report.py`).

______________________________________________________________________

## Phase 2: Parallel Subagent Research

Spawn **8 focused, load-balanced subagents concurrently in a single turn** (plus
extra sharded subagents if `fuzzilli-coverage.md` or `tests-coverage.md` has
`>4` files with `Needs Triage` lines—read-only subagents like `research-google`
cannot spawn child subagents themselves, so the **main agent** must shard
`Needs Triage` files across top-level subagents based on Phase 1's per-file
table).

Pass every subagent:

- Feature name, CLI flag, macro name, and comment block from
  `src/flags/feature-flags.h`
- Discovered bug IDs (`bugs.txt`), commit hashes (`commits.txt`), and
  `/tmp/v8-cov-<feature_name>/` path
- Instructions to keep the workspace strictly read-only and to **batch all
  independent `read_file`, `run_command`, and search calls within each turn**.

| Subagent                                | Template Sections Owned                                                                               | Key Checks & Commands                                                                                                                                                                                                                                                                                                                                           |
| :-------------------------------------- | :---------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **1 — Overview, Lifecycle & Checklist** | `Feature scope`, `Owner(s)`, `Feature flag`, `Tracking bug`, `Shipping step` (table), `Design docs`   | Resolve exact V8 versions for transition commits via `git show <hash>:include/v8-version.h` in one batched command; audit consistency against [references/shipping-steps.md](shipping-steps.md); search CLs/bugs/`v8-status-updates` for design docs.                                                                                                           |
| **2 — Chromestatus, Spec & Trials**     | `Shipping step` (trials), `Chrome launch process`, `Spec or proposal overview`, `Timeline and trials` | Run `read_finch_config.py --filter <keyword>` and `fetch_chromestatus.py <feature_name>` concurrently; check Origin Trial registration (`src/wasm/wasm-features.cc` or `src/execution/isolate.cc`); check [TC39](https://github.com/tc39/proposals) / [Wasm](https://github.com/WebAssembly/proposals) proposals and [Launch](https://launch.corp.google.com/). |
| **3 — Fuzzilli Coverage Triage**        | `Fuzzilli coverage`                                                                                   | Read `/tmp/v8-cov-<feature_name>/fuzzilli-coverage.md` (do **not** re-run coverage scripts). Inspect only `**[Needs Triage]**` blocks using embedded `[cov: N]` / `[UNCOV]` context counts and stop once the immediate condition is identified (see Section 2 below).                                                                                           |
| **4 — CI Test Coverage Triage**         | `Tests` (#4 `Test coverage`)                                                                          | Read `/tmp/v8-cov-<feature_name>/tests-coverage.md` (do **not** re-run coverage scripts). Cross-reference `**[Needs Triage]**` blocks against test flags and default CI variants and stop once the immediate condition is identified (see Section 2 below).                                                                                                     |
| **5 — Unit, Spec & WPT Tests**          | `Tests` (#1–#3)                                                                                       | Inspect test files from `commits.txt` and `.status` files; run available tests via `tools/dev/gm.py quiet x64.optdebug <suite>` (or `tools/run-tests.py --outdir=out/x64.optdebug <suite>`); compare spec tests against upstream (see Section 2 below).                                                                                                         |
| **6 — Fuzzers, Stress & Sanitizers**    | `Other fuzzer coverage`, `Stress testing`, `Other detection frameworks`                               | Check Wasm/JS fuzzers, Binaryen, FuzzTest, ClusterFuzz/Foozzie, stress flags/variants, sanitizers, and GCMole (see Section 3 below).                                                                                                                                                                                                                            |
| **7 — Bug History & Technical Risks**   | `Bug history`, `Risks`                                                                                | Batch `git show` on bug-fix/regression commits in `commits.txt` and `test/mjsunit/regress/`; synthesize technical risks and open `TODO`s (see Section 4 below).                                                                                                                                                                                                 |
| **8 — Metrics, Partners & Components**  | `Metrics`, `Partner outreach`, `Other Chromium components`                                            | Check `UseCounterFeature`, histograms, UKM, partner notes, and DevTools/inspector support (see Section 5 below).                                                                                                                                                                                                                                                |

______________________________________________________________________

## Detailed Research Instructions by Section

### 1. Chromestatus, Spec Proposals & Trials (Subagent 2)

- **Chromestatus (`fetch_chromestatus.py`):** Queries
  `https://chromestatus.com/api/v0/features` and strips the `)]}'\n` XSSI prefix
  automatically. If `search_web` returns an opaque grounding redirect URL
  (`https://vertexaisearch.cloud.google.com/grounding-api-redirect/...`, which
  often targets `https://*-dot-cr-status.appspot.com/feature/<id>`), pass the
  redirect URL directly to `fetch_chromestatus.py` (or use `--resolve-only`).
  Internal/optimization features without web-observable changes may have no
  Chromestatus entry and instead use a
  [Launch bug](https://launch.corp.google.com/).
- **Finch & Origin Trials:** Follow Section 2 of
  [references/shipping-steps.md](shipping-steps.md) to distinguish active
  google3 Finch GCL studies from Chromium `fieldtrial_testing_config.json`
  entries and Origin Trials.

### 2. Fuzzilli Coverage, CI Test Coverage & Test Suites (Subagents 3, 4, 5)

- **Coverage Triage (Subagents 3 & 4):**
  - Read `/tmp/v8-cov-<feature_name>/fuzzilli-coverage.md` and
    `/tmp/v8-cov-<feature_name>/tests-coverage.md` generated in Phase 1.
  - Distinguish benign uncovered code (`UNREACHABLE()`/`DCHECK`, non-x64/32-bit
    fallbacks, disassemblers, tracing/diagnostics, or libFuzzer-only generators
    in Fuzzilli) from genuine coverage gaps.
  - **Stop once the immediate condition is identified (explicitly include this
    in the subagent prompt):** Use the embedded `[cov: N]` vs. `[UNCOV]`
    surrounding context lines to determine whether the enclosing function ran
    and which immediate branch, overload, or test flag (such as `--no-liftoff`)
    was missed. Once the immediate condition is identified, record it and move
    on—do **not** trace transitive root causes across unrelated compiler passes,
    IR effect definitions (`OpEffects`), or generic instruction-selector helpers
    (such as `CanCover`).
  - Note on Wasm tier coverage in `V8 Linux64 - coverage`: While CI tests run
    both with `--liftoff-only` and `--no-liftoff`, the coverage report only
    records the default configuration behavior. A lack of coverage in one tier
    alone is therefore likely not actionable (and should be counted as benign
    when covered by the other tier on standard CI bots).
  - **Report only the final post-triage numbers:** `Auto-Benign` vs.
    `Needs Triage` in `fuzzilli-coverage.md` and `tests-coverage.md` is purely
    an internal pre-filter to reduce subagent work. In the synthesized launch
    review (Executive summary, Overall bullets, Per-file tables, and Triage
    summaries), merge script-triaged and subagent-triaged benign lines into a
    single **Benign** column (`Benign` + `Genuine Gaps` = `Uncovered (Raw)`),
    and report a single final **Eff. Cov %**
    (`Covered / (Executable - Benign)`). Do **not** expose intermediate
    pre-triage percentages or distinguish "auto-benign" from "manual benign" in
    the final report.
- **Unit, Spec & WPT Tests (Subagent 5):**
  - **V8 unit/integration tests:** Check
    `test/{mjsunit,unittests,cctest,inspector,intl}/` and corresponding
    `.status` files (`mjsunit.status`, `unittests.status`, `cctest.status`) to
    verify tests are not skipped or failing. If an `out/` build exists (or when
    verifying locally), run tests via
    `tools/dev/gm.py quiet x64.optdebug <test_target>` (or
    `tools/run-tests.py --outdir=out/x64.optdebug <test_target>`).
  - **Spec tests:**
    - *JavaScript:* Check `test/test262/` (`local-tests/`, staging tests) and
      `test/test262/test262.status`.
    - *WebAssembly:* Check `test/wasm-spec-tests/tests/proposals/<repo>/`
      (`wasm-spec-tests.status`) and `test/wasm-js/tests/proposals/<repo>/`
      (`wasm-js.status`), plus `tools/wasm/update-wasm-spec-tests.sh` and
      `proposal_flags` in `test/{wasm-spec-tests,wasm-js}/testcfg.py`. Compare
      V8's spec tests against the upstream proposal's `test/core` and
      `test/js-api` directories. *(Never run
      `tools/wasm/update-wasm-spec-tests.sh` during a review—it wipes local
      directories, requires OCaml, and uploads to GCS; add an action item
      instead.)*
  - **Web Platform Tests (WPTs):** Check `test/wasm-js/tests/wpt/` in V8 and
    `third_party/blink/web_tests/external/wpt/` (or upstream
    [WPT](https://github.com/web-platform-tests/wpt)) if Blink/Web APIs are
    involved.

### 3. Other Fuzzers, Stress Testing & Detection Frameworks (Subagent 6)

- **Other fuzzer coverage:** Inspect `commits.txt` CLs and codebase support in:
  - *Wasm fuzzers:* `src/wasm/fuzzing/random-module-generation.cc`,
    `test/common/wasm/fuzzer-common.cc`, `test/fuzzer/wasm/`.
  - *Binaryen fuzzer:*
    [WebAssembly/binaryen](https://github.com/WebAssembly/binaryen) (for Wasm
    features).
  - *FuzzTest / Centipede:* `test/unittests/**/*fuzztest*.cc`.
  - *ClusterFuzz JS & differential fuzzing:* `tools/clusterfuzz/js_fuzzer/` and
    `tools/clusterfuzz/foozzie/` (`v8_fuzz_flags.json`, `v8_fuzz_config.py`).
- **Stress testing:** Check `src/flags/flag-definitions.h`,
  `tools/testrunner/local/variants.py`,
  `tools/clusterfuzz/foozzie/v8_fuzz_flags.json`, and
  `tools/clusterfuzz/trials/clusterfuzz_trials_config.json` for stress modes
  (`--stress-*`, `--deopt-every-n-times`, `--liftoff-only`).
- **Other detection frameworks:** Check `commits.txt` and `.status` files for
  ASAN, MSAN, TSAN, UBSan (`src/base/sanitizer/`), and GCMole (`tools/gcmole/`,
  `DisallowGarbageCollection`).

### 4. Bug History & Technical Risks (Subagent 7)

- **Bug history:** Inspect all bug-fix/regression commits in `commits.txt` and
  `test/mjsunit/regress/` in a single batched `git show` command; populate the
  Bug ID, short description, priority, status, and fuzzer/test source.
- **Risks:** Synthesize technical risks from implementation complexity, past
  regression patterns (e.g., register allocation, multi-output IR nodes,
  architecture ports, stack slots), and open `TODO`s; keep the fallback checkbox
  categories from
  [assets/launch-review-template.md](../assets/launch-review-template.md) if no
  specific risks are found.

### 5. Metrics, Partner Outreach & Other Chromium Components (Subagent 8)

- **Metrics:** Check `v8::Isolate::UseCounterFeature` in `include/v8-isolate.h`,
  `kUseCounters` / `check_use_counter` in `src/wasm/module-compiler.cc`,
  histograms in `src/logging/counters-definitions.h`, and UKM in
  `include/v8-metrics.h`.
- **Partner outreach:** Check tracking bug and Chromestatus comments for partner
  mentions; include the template checkboxes if unknown.
- **Other Chromium components:** Check `src/inspector/`, `test/inspector/`,
  `src/wasm/wasm-disassembler.cc`, and debugging checklists in
  `docs/feature-launch-process.md` and `docs/wasm-shipping-checklist.md`.

______________________________________________________________________

## Phase 3: Synthesis & Action Items (Main Agent)

Copy the skeleton from
[assets/launch-review-template.md](../assets/launch-review-template.md), fill in
all sections from Subagents 1–8, consolidate every open `tba` choice and
discovered engineering/process gap into the final **Action items** checklist,
and save the completed report to `/tmp/launch-review-<feature-name>.md` (or the
user-specified path).
