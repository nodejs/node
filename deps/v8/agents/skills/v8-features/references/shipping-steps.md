# V8 Feature Status, Consistency & Step Advancement

This reference covers determining a V8 feature's shipping status, checking
consistency against V8's shipping checklists, and advancing a feature to the
next step.

## 1. Feature Flag Macros & CLI Flag Mapping (`src/flags/feature-flags.h`)

All V8 features following the launch process are tracked in
`src/flags/feature-flags.h` under one of four shipping-step macros:

| Shipping Step                | Macro in `src/flags/feature-flags.h` | Default State                                         |
| :--------------------------- | :----------------------------------- | :---------------------------------------------------- |
| **Experimental (Inception)** | `FOREACH_EXPERIMENTAL_FEATURE_FLAG`  | Disabled by default (in-progress)                     |
| **Pre-staged**               | `FOREACH_PRE_STAGED_FEATURE_FLAG`    | Disabled by default; ready for fuzzing / VRP          |
| **Staged**                   | `FOREACH_STAGED_FEATURE_FLAG`        | Disabled by default; enabled via `--future` / staging |
| **Shipped**                  | `FOREACH_SHIPPED_FEATURE_FLAG`       | Enabled by default                                    |

Each entry uses one of three type macros (optionally wrapped in a build-flag
conditional such as `IF_INTL_ENABLED`, `IF_REVEC_ENABLED`, or
`IF_SPARKPLUG_PLUS_ENABLED`):

- `WASM_FEATURE(foo_bar, "description")` -> CLI flag `--wasm-foo-bar`
  (`v8_flags.wasm_foo_bar`)
- `JS_FEATURE(foo_bar, "description")` -> CLI flag `--foo-bar`
  (`v8_flags.foo_bar`)
- `INTERNAL_FEATURE(foo_bar, "description")` -> CLI flag `--foo-bar`
  (`v8_flags.foo_bar`)

> [!IMPORTANT] `docs/feature-launch-process.md` and
> `docs/wasm-shipping-checklist.md` still reference legacy pre-`2628639aef5`
> files and macros (`src/wasm/wasm-feature-flags.h`, 3-parameter
> `FOREACH_WASM_*`, `JAVASCRIPT_STAGED_FEATURES_BASE`).
> `src/flags/feature-flags.h` supersedes those legacy macro definitions.

## 2. Determining Feature Status, Trials & External Metadata

1. **Macro & Comment Block (`src/flags/feature-flags.h`):** Check which
   `FOREACH_*_FEATURE_FLAG` macro contains the feature and inspect the preceding
   comment block for:
   - Feature description / internal name
   - Proposal link (`https://github.com/tc39/...` or
     `https://github.com/WebAssembly/...`)
   - V8-side owner(s)
   - Staging/shipping milestone or trial annotations (e.g.,
     `/* Staged in 15.7 */`)
2. **Finch Studies & FieldTrial Testing Config:** Run:
   ```bash
   python3 agents/skills/v8-features/scripts/read_finch_config.py [--filter <pattern>]
   ```
   This inspects both:
   - **Google3 Finch GCL studies**
     (`/google/src/files/head/depot/google3/googledata/googleclient/chrome/finch/gcl_studies/`,
     when accessible on a Googler machine) for active Finch rollouts.
   - **Chromium FieldTrial testing config**
     (`testing/variations/fieldtrial_testing_config.json`), noting enabled vs.
     disabled (kill-switch) states. *(A fieldtrial config is required for Finch,
     but presence in `fieldtrial_testing_config.json` alone does not mean an
     active Finch study is running.)*
3. **Origin Trial Status:**
   - **WebAssembly features:** Check `WasmEnabledFeatures::FromContext` in
     `src/wasm/wasm-features.cc`.
   - **JavaScript features:** Check `Isolate::InstallConditionalFeatures` in
     `src/execution/isolate.cc`.
4. **Chromestatus Entry:** If not linked in `src/flags/feature-flags.h`, query
   Chromestatus (or resolve `search_web` redirect URLs) via:
   ```bash
   python3 agents/skills/v8-features/scripts/fetch_chromestatus.py <feature_name_or_id_or_url>
   ```
5. **Transition History across Flag Files:** Always use `git log -G` (diff regex
   search) rather than `git log -S` or `git blame` (which breaks on bulk
   refactor `2628639aef5` and multiline macro `\` continuations):
   ```bash
   git log -G"\b<feature_name>\b" -p -- src/flags/feature-flags.h src/wasm/wasm-feature-flags.h src/flags/flag-definitions.h
   ```

## 3. Step Consistency Checklist

Cross-reference the feature's current (or target) step against these codebase
requirements:

- **All tracked features (`src/flags/feature-flags.h`):**
  - Preceded by a comment block with description, proposal URL (if applicable),
    V8 owner, and trial/milestone notes.
- **JavaScript features (`JS_FEATURE`):**
  - `Genesis::InitializeGlobal_<id>()` is defined in `src/init/bootstrapper.cc`
    (required whenever a `JS_FEATURE` is in `src/flags/feature-flags.h`).
- **WebAssembly features (`WASM_FEATURE`) in Pre-staged or later:**
  - Proposal repository is listed in `repos` in
    `tools/wasm/update-wasm-spec-tests.sh`.
  - Feature flag is registered in `proposal_flags` in both
    `test/wasm-spec-tests/testcfg.py` and `test/wasm-js/testcfg.py`.
- **WebAssembly features (`WASM_FEATURE`) in Staged or Shipped:**
  - Mapped to a `v8::Isolate::UseCounterFeature` in `kUseCounters` inside
    `src/wasm/module-compiler.cc` (or explicitly listed in
    `kIntentionallyNoUseCounter`), enforced at compile time by
    `static_assert(check_use_counter(WasmDetectedFeature::feat))`.
- **Flags outside `src/flags/feature-flags.h`:**
  - Helper or non-V8-tracked flags may live directly in
    `src/flags/flag-definitions.h`. If asked to advance one, ask the user
    whether it should be migrated to `src/flags/feature-flags.h`. Query the user
    before overriding any intentional deviation from the documented process.

## 4. Procedure to Advance a Feature Step

1. **Verify Consistency:** Audit the feature against Section 3 above (using
   parallel read-only subagents if advancing multiple features).
2. **Apply Code Changes:**
   - Move the feature entry to the target `FOREACH_<STEP>_FEATURE_FLAG` macro in
     `src/flags/feature-flags.h` and update its milestone/status comment.
   - Update `src/init/bootstrapper.cc`, `src/wasm/module-compiler.cc`,
     `test/wasm-spec-tests/testcfg.py`, and/or `test/wasm-js/testcfg.py` if
     required by the target step.
3. **Build & Run Verification Tests:**
   - Compile and run the relevant test suites using `gm.py` (which builds and
     runs tests in `out/`):
     ```bash
     tools/dev/gm.py quiet x64.optdebug <suite>
     ```
     (e.g., `wasm-spec-tests wasm-js` for WebAssembly features, or
     `mjsunit test262` for JavaScript features; or run
     `tools/run-tests.py --outdir=out/x64.optdebug <suite>` once built).
   - Ensure the build succeeds (including `check_use_counter` static assertions)
     and all tests pass.
