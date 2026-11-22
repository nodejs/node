# V8 Launch Review: `<Feature Name>`

## Executive summary

- **Status:** `<Current shipping step>`
- **Fuzzilli coverage:** `<covered>/<executable>` (`<raw_pct>%` raw;
  `<eff_pct>%` effective)
- **Test coverage (`V8 Linux64 - coverage`):** `<covered>/<executable>`
  (`<raw_pct>%` raw; `<eff_pct>%` effective)
- **Key findings & risks:**
  `<1-3 sentence summary of actionable items, coverage gaps, and technical risks>`

## Overview

### Feature scope

`<Brief summary of what the feature does and why>`

### Owner(s)

- **V8 owner(s):** `<owners from src/flags/feature-flags.h and CL authors>`

### Feature flag

- **CLI flag:** `--<flag-name>` (`v8_flags.<flag_id>`)
- **Macro definition:**
  `<JS_FEATURE|WASM_FEATURE|INTERNAL_FEATURE>(<flag_id>, "<description>")` in
  `src/flags/feature-flags.h`

### Tracking bug

- **Primary tracking bug:**
  `<https://issues.chromium.org/issues/<id> (or b/<id>)>`
- **Related sub-bugs:** `<None or list of sub-bugs>`

### Shipping step

- **Current step:** `<Experimental | Pre-staged | Staged | Shipped>`
  (`FOREACH_<STEP>_FEATURE_FLAG` in `src/flags/feature-flags.h`)
- **Finch / FieldTrial status:**
  `<Active Finch GCL study and/or Chromium fieldtrial_testing_config.json status, or None>`
- **Origin Trial status:**
  `<Origin Trial registration in src/wasm/wasm-features.cc or src/execution/isolate.cc, or None>`

| Step                     | Commit / CL               | Date           | V8 Version | Author     |
| :----------------------- | :------------------------ | :------------- | :--------- | :--------- |
| Inception (Experimental) | `<hash / CL link>`        | `<YYYY-MM-DD>` | `<X.Y.Z>`  | `<author>` |
| Pre-staged               | `<hash / CL link or N/A>` | `<YYYY-MM-DD>` | `<X.Y.Z>`  | `<author>` |
| Staged                   | `<hash / CL link or N/A>` | `<YYYY-MM-DD>` | `<X.Y.Z>`  | `<author>` |
| Shipped                  | `<hash / CL link or N/A>` | `<YYYY-MM-DD>` | `<X.Y.Z>`  | `<author>` |

### Chrome launch process

- **Chromestatus entry:** `<https://chromestatus.com/feature/<id> or None>`
- **Launch bug:** `<https://launch.corp.google.com/launch/<id> or None>`

### Spec or proposal overview

- **Specification / Proposal:**
  `<link to TC39 / WebAssembly proposal repo or PR>`

### Design docs

- [ ] Link design doc(s): `tba`
- [ ] None

## Fuzzing and testing

### Fuzzilli coverage

- **Coverage revision:** `Chromium <hash>` / `V8 <hash>`
- **Overall:** `<covered>/<executable>` executable lines covered (`<raw_pct>%`
  raw; `<eff_pct>%` excluding benign)

| File                | Total Modified | Executable | Covered | Uncovered (Raw) | Benign | Genuine Gaps | Raw Cov % | Eff. Cov % |
| :------------------ | -------------: | ---------: | ------: | --------------: | -----: | -----------: | --------: | ---------: |
| `<path/to/file.cc>` |          `<N>` |      `<N>` |   `<N>` |           `<N>` |  `<N>` |        `<N>` |  `<X.Y>%` |   `<X.Y>%` |

**Triage of uncovered blocks:**

- `<Brief summary of benign blocks and detailed analysis of any genuine coverage gaps>`

### Other fuzzer coverage

- **Wasm fuzzers (`random-module-generation.cc`, `test/fuzzer/wasm/`):**
  `<status>`
- **Binaryen fuzzer:** `<status or N/A>`
- **FuzzTest / Centipede (`test/unittests/**/*fuzztest*.cc`):** `<status>`
- **ClusterFuzz (`js_fuzzer`, `foozzie`):** `<status>`

### Tests

1. **V8 unit and integration tests:**
   `<list of mjsunit/unittests/cctest/inspector/intl files and .status check>`
2. **Spec tests:**
   `<test262 or wasm-spec-tests / wasm-js status and upstream comparison>`
3. **Web Platform Tests (WPTs):** `<WPT coverage status>`
4. **Test coverage (`V8 Linux64 - coverage`):**
   - **Build:** `[Build #<num>](<report_url>/index.html)` (`V8 <hash>`)
   - **Overall:** `<covered>/<executable>` executable lines covered
     (`<raw_pct>%` raw; `<eff_pct>%` excluding benign)

| File                | Total Modified | Executable | Covered | Uncovered (Raw) | Benign | Genuine Gaps | Raw Cov % | Eff. Cov % |
| :------------------ | -------------: | ---------: | ------: | --------------: | -----: | -----------: | --------: | ---------: |
| `<path/to/file.cc>` |          `<N>` |      `<N>` |   `<N>` |           `<N>` |  `<N>` |        `<N>` |  `<X.Y>%` |   `<X.Y>%` |

**Triage of uncovered test blocks:**

- `<Summary of benign vs. genuine test coverage gaps>`

### Stress testing

`<2-3 sentence summary of relevant stress flags, variants, and fuzzers>`

### Other detection frameworks

`<2-3 sentence summary of ASAN, MSAN, TSAN, UBSan, and GCMole coverage/annotations>`

### Bug history

| Bug ID     | Short Description | Priority  | Status                  | Found By                                        |
| :--------- | :---------------- | :-------- | :---------------------- | :---------------------------------------------- |
| `<bug_id>` | `<description>`   | `<P0-P4>` | `<Fixed/Verified/Open>` | `<Fuzzilli / ClusterFuzz / Unit test / Manual>` |

## Rollout plan

### Timeline and trials

- [ ] Ship directly without a trial (Target milestone: `tba`)
- [ ] Origin Trial (Target milestones: `tba`)
- [ ] Finch / field trial (Target milestones: `tba`)
- [ ] Developer trial only (Target milestones: `tba`)

### Risks

- [ ] None known
- [ ] Performance regressions: `tba`
- [ ] Memory overhead: `tba`
- [ ] Web / backwards compatibility: `tba`
- [ ] Security / attack surface: `tba`

### Metrics

- **UseCounter (`include/v8-isolate.h`, `src/wasm/module-compiler.cc`):**
  `<UseCounterFeature or None>`
- **Histograms (`src/logging/counters-definitions.h`) & UKM
  (`include/v8-metrics.h`):** `<metrics or None>`

### Partner outreach

- [ ] None / No specific partner outreach required
- [ ] Partner outreach planned or completed (Partners and status: `tba`)

## Other Chromium components

- [ ] None / No other Chromium components affected
- [ ] DevTools / debugging support (Status / checklist review: `tba`)
- [ ] Blink bindings or other Chromium components (Components and review status:
  `tba`)

## Action items

- [ ] **Report completion:** Confirm or fill in `tba` options above (Design
  docs, Timeline and trials, Risks, Partner outreach, Other Chromium
  components).
- [ ] **Engineering & process prerequisites:**
  `<list concrete gaps found during review>`
- [ ] **Launch review:** Share this document with the Bug Detection team and
  relevant TLs, and schedule a launch review meeting or request an offline
  review.
