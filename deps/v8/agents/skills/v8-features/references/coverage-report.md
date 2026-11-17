# V8 Coverage Report Generation & Triage

Generate line coverage reports for V8 features, tracking bugs, Change-Ids, or
commit hashes using Fuzzilli x64 coverage and/or the
[V8 Linux64 - coverage](https://ci.chromium.org/ui/p/v8/builders/ci/V8%20Linux64%20-%20coverage)
CI builder.

## 1. Automated Report Generation (Recommended)

Run `generate_coverage_report.py` from the V8 repository root:

```bash
python3 agents/skills/v8-features/scripts/generate_coverage_report.py \
  [<reference1> ...] \
  [--feature <feature_name>] \
  [--grep "<pattern>"] \
  [--source {fuzzilli,tests,ci,both}] \
  [--output-dir /tmp/v8-cov-<name>]
```

### Key Options

- `<reference>`: Git commit hash, Gerrit Change-Id (`Iabc123...`), Buganizer /
  `crbug.com` ID or URL (`123456`, `b/123456`, `v8:12345`,
  `https://crbug.com/...`), or V8 feature flag name (`wide_arithmetic`).
- `--feature` / `-f`: Traces the feature flag across
  `src/flags/feature-flags.h`, `src/wasm/wasm-feature-flags.h`, and
  `src/flags/flag-definitions.h` (`git log -G`), automatically resolving all
  implementation commits and `Bug:` / `Fixed:` IDs in parallel.
- `--grep` / `-g`: Includes commits whose commit message matches the regex
  (e.g., `"\\[wasm-wide-arith\\]"`).
- `--source` / `-s`:
  - `fuzzilli` (default): Chromium Fuzzilli x64 coverage dashboard.
  - `tests` (or `ci`): Latest `V8 Linux64 - coverage` CI build (`llvm-cov` HTML
    report in GCS).
  - `both`: Fetches and analyzes both Fuzzilli and CI test coverage
    **concurrently in parallel**, reusing the resolved commit hashes.
- `--output-dir` / `-o`: Writes `commits.txt`, `bugs.txt`,
  `fuzzilli-coverage.md`, and/or `tests-coverage.md` to the target directory
  (always use `/tmp/...` or the agent scratch directory, never the git working
  tree).
- `--no-snippets`: Omits C++ source snippets underneath uncovered blocks while
  retaining per-file `Auto-Benign` vs. `Needs Triage` counts and block tags.
- `--checkout`: Checks out the coverage revision in the working tree. *(Omit
  `--checkout` when running parallel subagents; the script inspects git history
  and blames against the target revision without mutating `HEAD`.)*

## 2. Triaging Uncovered Code Blocks & Parallelization

- **Linux x64 scope:** Both coverage sources run only on Linux x64. Non-x64
  backend files (`arm`, `arm64`, `ia32`, `loong64`, `mips64`, `ppc64`,
  `riscv32`, `riscv64`, `s390x`) and `test/`/`tools/` files are skipped
  automatically before `git blame`.
- **Focus strictly on `**[Needs Triage]**` blocks:**
  `generate_coverage_report.py` tracks `#if`/`#else`/`#endif` nesting and
  automatically classifies common benign blocks (`UNREACHABLE()`, `DCHECK`,
  `PrintOptions`, `inputs_rep()`, 32-bit/non-x64 preprocessor branches, and Wasm
  libFuzzer generators under `fuzzilli`).
- **Use embedded `[cov: N]` vs. `[UNCOV]` context lines:** Each uncovered block
  includes `±2` surrounding lines with execution counts so you can see
  immediately whether the enclosing function ran and which branch was missed
  (e.g., an instruction-selector immediate/memory branch, or a test running only
  in Liftoff without `--no-liftoff`). Once the immediate condition for a
  coverage gap is identified, record it and move on without tracing unrelated
  compiler passes.
- **Shard across top-level subagents:** If more than 3–4 files have
  `Needs Triage` lines in the per-file table, the main agent should partition
  those files by subsystem (e.g., `src/wasm/`, `src/compiler/turboshaft/`,
  `src/compiler/backend/`) across parallel subagents.
- **Report only the final post-triage numbers:** `Auto-Benign` vs.
  `Needs Triage` in the script output is purely an internal pre-filter. Once the
  `**[Needs Triage]**` blocks have been classified into benign vs. genuine gaps,
  present a single combined **Benign** count, **Genuine Gaps** count, and final
  post-triage **Effective Coverage %** (`Covered / (Executable - Benign)`). Do
  not distinguish "auto-benign" from "manual benign" or report pre-triage
  intermediate effective percentages in the final user-facing report.

## 3. Debugging Individual Pipeline Steps

If you need to debug a specific phase of `generate_coverage_report.py` without
mutating `HEAD`:

1. **Map Chromium hash to V8 revision (Fuzzilli):**
   `python3 agents/skills/v8-features/scripts/get_v8_revision.py <chromium_hash>`
2. **Fetch the target V8 revision into the object store (no checkout):**
   `git fetch --no-write-fetch-head origin <v8_hash>`
3. **Resolve commit hashes or blame affected blocks at `<v8_hash>`:**
   `python3 agents/skills/v8-features/scripts/find_affected_blocks.py --revision <v8_hash> [--hashes-only] [--feature <name>] [<ref> ...]`
4. **Extract line counts from a downloaded coverage HTML file in `/tmp`:**
   `python3 agents/skills/v8-features/scripts/extract_coverage.py /tmp/<file>.html <start>-<end> [...]`
   *(Supports both Chromium Fuzzilli HTML and V8 CI `llvm-cov` HTML; outputs
   `line: count` where `0` = uncovered, `>0` = covered, `None` =
   non-executable).*
