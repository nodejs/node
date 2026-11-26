---
name: fuzzing-cls-with-redline
description: >-
  Builds, configures, starts, and monitors V8 fuzzing with Redline for a Gerrit
  CL, local branch, or uncommitted patch on a Cloudtop. Translates V8 code
  changes into crash-finding builds and hourly bug reports for engineers with no
  fuzzing experience. Use this skill when a V8 engineer wants to fuzz or
  stress-test a CL, branch, or patch, or check and stop active CL fuzzing runs.
  Do not use this skill to edit Redline (`experimental/redline/`), run
  Fuzzilli, run `run-tests.py` suites, or run A/B benchmarks. Do not use it to
  write `test/fuzzer/` targets, minimize one testcase, or reproduce a
  ClusterFuzz issue.
---

# Fuzz V8 CLs with Redline

Run V8 fuzzing on a local or remote Cloudtop for a V8 C++ change. Present all
questions, status tables, and crash commands in V8 terms only.

## Mode Dispatch

Choose the path from the user request:

| User Request                | Path                                                                                 |
| --------------------------- | ------------------------------------------------------------------------------------ |
| Start a new fuzzing run     | Run Steps 1 to 6.                                                                    |
| Check an active fuzzing run | Run `prepare_run.py probe` (locally or over SSH) to find `$RUN_ROOT`, then Step 6.2. |
| Stop an active fuzzing run  | Run `prepare_run.py probe` (locally or over SSH) to find `$RUN_ROOT`, then Step 6.4. |

## Step 1: Collect Inputs, Examine the Host, and Analyze the Diff

Keep user input minimal. Infer defaults before you ask questions.

1. Find the V8 root with `V8_ROOT=$(git rev-parse --show-toplevel)`. Make sure
   that `src/d8/d8.cc` exists there. Set `SKILL_DIR` to the absolute path of
   this skill directory.
2. Identify the target change:
   - If the user gives a Gerrit CL number or URL, extract the numeric `<CL_ID>`.
   - If the user names a local branch, use that branch.
   - If the user gives no change, use the current branch (`HEAD`).
3. Identify the host and examine capacity:
   - Run `python3 "$SKILL_DIR/scripts/prepare_run.py" probe` locally first.
   - If the user gave no remote host and `is_cloudtop` is `true`, run locally
     (`HOST=local`) and reuse a worktree of `$V8_ROOT`.
   - If `is_cloudtop` is `false` and the user gave no remote host, ask for the
     Cloudtop hostname (for example, `user.c.googlers.com`).
   - On a remote Cloudtop, open the SSH `ControlMaster` and pipe
     `prepare_run.py probe` over SSH per
     [references/build-and-cloudtop.md](references/build-and-cloudtop.md)
     Section 1.
   - Record `TARGET_HOME`, `HOST_NPROC`, `HOST_RAM_GB`, and `free_disk_gb`. If
     `free_disk_gb` is less than 30, warn the user before you build.
4. Prepare and analyze the diff against `origin/main`:
   - **Local branch**: Run `git -C "$V8_ROOT" fetch origin main`. If the target
     branch is `HEAD`, run `git -C "$V8_ROOT" status --short` to find
     uncommitted edits (include them with `--include-worktree` when asked). Then
     run:
     ```shell
     WORK_DIR=$(mktemp -d /tmp/cl-fuzz-XXXXXX)
     python3 "$SKILL_DIR/scripts/analyze_diff.py" \
       --repo "$V8_ROOT" --base origin/main --head "<ref>" \
       --vcpus "$HOST_NPROC" --ram-gb "$HOST_RAM_GB" \
       --duration-sec 43200 --json > "$WORK_DIR/plan.json"
     ```
   - **Gerrit CL**: Create a temporary worktree in `$WORK_DIR/wt`. Apply the CL
     and run `analyze_diff.py`:
     ```shell
     WORK_DIR=$(mktemp -d /tmp/cl-fuzz-XXXXXX)
     git -C "$V8_ROOT" fetch origin main
     git -C "$V8_ROOT" worktree add --detach "$WORK_DIR/wt" origin/main
     (cd "$WORK_DIR/wt" && git cl patch -f "<CL_ID>")
     python3 "$SKILL_DIR/scripts/analyze_diff.py" \
       --repo "$WORK_DIR/wt" --base origin/main --head HEAD \
       --vcpus "$HOST_NPROC" --ram-gb "$HOST_RAM_GB" \
       --duration-sec 43200 --json > "$WORK_DIR/plan.json"
     ```
   - If `git cl patch -f` fails, stop and report the error to the user.
5. Read the diff hunks (`git diff origin/main...<ref>`) to find V8 clues, new
   flags, and changed `test/mjsunit/` files.
6. Read `has_only_non_runtime` and `unsupported_arches` in
   `$WORK_DIR/plan.json`. If no `d8` runtime code changed (or only unsupported
   architecture code changed), tell the user and ask whether to stop or fuzz a
   specific V8 subsystem.

## Step 2: Review Defaults with Minimal V8 Questions

Ask at most one round of questions. Use V8 C++ terms only: CLI flags, compiler
tiers, execution modes, `test/mjsunit/` files, duration, and JS or Wasm
constructs.

- If the user asked to start immediately with defaults, skip to Step 3.
- Propose the planned builds, V8 flags, target subsystems, and default 12 h
  duration in one short summary.
- Start from `v8_questions` in `$WORK_DIR/plan.json`. Keep only questions that
  the diff does not already answer (always ask or review the 12 h default
  duration).
- When choices are clear, use an interactive selector (such as `ask_question`)
  with a `"Start with these defaults (12 h)"` option first.
- If `supports_bundle_reprl` or `supports_v8_redline` is `false`, tell the user
  that the run will rebase a separate copy onto `origin/main` so that crash
  reproducers work.

**Example V8 Review Prompt:**

> Ready to fuzz your Maglev change (`src/maglev/maglev-graph-builder.cc`)
> locally on this Cloudtop with 3 builds (ASan with slow DCHECKs, CL-targeted
> slow DCHECKs, and Turbolev verification).
>
> 1. Your CL adds `--maglev-new-opt` (default `false`). Enable it on all builds
>    (Recommended), or test both on and off?
> 2. Default run duration is 12 h. Keep 12 h (Recommended), or choose 4 h, 24 h,
>    or until stopped?

## Step 3: Finalize `plan.json`

Read [references/folder-mapping.md](references/folder-mapping.md) when you
adjust flags, generator weights, or initial test directories beyond
`$WORK_DIR/plan.json`. Update `$WORK_DIR/plan.json` with the answers from the
user:

- Keep at least one full-V8 `asan_slow_dcheck` arm (`change_based: false`,
  `builtins_coverage: true`) to catch memory safety and `SLOW_DCHECK` bugs.
- If `has_x64_d8_cpp` is `true`, keep `change_based: true` on
  `counters_slow_dcheck`. If only `arm64/` changed, set `change_based: true` on
  `arm64sim`.
- If the diff changes only `.tq` or `src/builtins/*-gen.cc`, set
  `change_based: false` and `builtins_coverage: true` on all arms.
- If the user does not request fixed flags, keep `randomize_d8_flags: true`
  (except on `runtime` preset arms). Set `d8_flag_profile` to `"turbolev"` for
  Turbolev, `"maglev"` for Maglev, and `"fuzzilli"` for general V8, compiler,
  Wasm, or sandbox arms.
- Put custom flags in `d8_extra_args` (JSON string array). Set `seeds` to a
  comma-separated string of `test/mjsunit/` paths.
- To test a new CL flag off on a change-based arm, add `--<flag>` to
  `excluded_flags` (or `--no-<flag>` to `d8_extra_args`).
- Set `duration_sec` on each arm to the chosen duration (default `43200` for 12
  h, `0` for until stopped).
- Run a dry run before you build:
  `python3 "$SKILL_DIR/scripts/launch_arms.py" --v8-dir "$V8_ROOT" --run-root "$WORK_DIR/dry" --plan "$WORK_DIR/plan.json" --dry-run`.

## Step 4: Sync, Build, Stage, and Start Arms

Read [references/build-and-cloudtop.md](references/build-and-cloudtop.md) and
run each gate in order. If any gate fails, stop immediately.

1. **Sync the target change**: Prepare the isolated checkout (`$TARGET_V8`:
   local worktree `$TARGET_HOME/cl-fuzz/v8-wt` when `HOST=local`, or
   `$TARGET_HOME/cl-fuzz/v8` when remote). Apply and commit the change onto
   branch `cl-fuzz-target`. If `git apply` or rebase has conflicts, stop and
   report the files with conflicts.
2. **Build binaries**: Run `scripts/prepare_run.py build` on `$TARGET_V8` with
   `plan.json`. If compilation fails, stop and show the compiler errors.
3. **Stage binaries and run `selftest`**: Run `scripts/prepare_run.py stage` to
   populate `$RUN_ROOT/bin/`, compile `$RUN_ROOT/nodump.so`, and run `selftest`.
   If any non-timing test fails, stop and diagnose before you start fuzzing.
4. **Start arms**: Run `scripts/launch_arms.py` with `$RUN_ROOT/plan.json`. Wait
   10 s.
5. **Remove local scratch worktree**: If Step 1 created `$WORK_DIR/wt`, remove
   it with `git -C "$V8_ROOT" worktree remove --force "$WORK_DIR/wt"`.

## Step 5: Make Sure That Arms Started and Show the Setup Table

Run `scripts/report_arms.py "$RUN_ROOT" --mode immediate --host "$HOST"`
(locally, or over SSH with `-o BatchMode=yes -o ConnectTimeout=10`).

- **Gate**: Make sure that every arm shows `RUNNING`. If any arm shows
  `STOPPED`, read the tail of `$RUN_ROOT/arms/<arm>/redline.log`. Fix the cause,
  then restart stopped arms with `launch_arms.py --only-stopped`.
- Show the Markdown setup table to the user in plain V8 terms.
- Tell the user how to view status or stop the run early.

## Step 6: Monitor Hourly and Report All Bugs

1. **Schedule hourly reports**: Copy the template in
   [assets/hourly-prompt.md](assets/hourly-prompt.md) and fill in literal
   `HOST`, `RUN_ROOT`, `CTL`, and `SKILL_DIR` values. Schedule it once per hour
   with a recurring scheduler tool (for example, `schedule` with
   `IsDaemon: false`). If no scheduler tool exists, give the user the one-line
   status command.
2. **Collect hourly status**: Run
   `scripts/report_arms.py "$RUN_ROOT" --mode hourly --host "$HOST"`. Pass
   `-o BatchMode=yes -o ConnectTimeout=10` on SSH calls.
   - If SSH exits with an authentication error, give the user the one-line
     command to renew SSH access.
3. **Report all crash classes**:
   - Report every unique crash signature across all arms. Include
     `deterministic`, `state-dependent`, `amplified`, `flaky`, and `in-flight`
     crashes (even when `reproduced` is `0`).
   - Show the exact reproduction command and state clearly whether it runs on
     the Cloudtop. For remote runs, also show the `scp` command to copy the
     testcase locally.
   - For **state-dependent** crashes, show `d8 ... --bundle-reprl <bundle_file>`
     (with `.bundle.min.js` or `.bundle.js`) and note that `--bundle-reprl`
     replays the short script sequence in one `d8` process.
   - For **flaky** crashes, show `d8_random_args`, the first stderr backtrace
     lines, the `.js` and `.txt` paths, and the crash-signal retry loop
     (`rc > 128`).
   - If any of these events occurs, show an alert:
     - An arm stopped without a clean `end` event.
     - All initial tests failed at startup.
     - The timeout rate exceeds 15%.
     - CL target coverage stays at `0` after 15 min.
4. **Stop the run**: When all arms end or when the user asks to stop, run
   `scripts/prepare_run.py stop --run-root "$RUN_ROOT"`. Then cancel the hourly
   schedule (for example, with `manage_task`).
