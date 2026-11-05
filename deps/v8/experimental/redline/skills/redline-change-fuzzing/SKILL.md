---
name: redline-change-fuzzing
description: Runs and checks the change-based fuzzing of Redline, which gives more fuzzing time to the code of one change. Covers the targets file, the status line, the weights and the focus. Lists the causes of a session without targets, or with targets that no run reaches. Shows how to compare change-based fuzzing with normal mode. Use it for all work on change-based fuzzing.
---

# Change-based fuzzing

The change-based fuzzing section of TUTORIAL.md gives the commands, and
DESIGN.md section 5.4 explains the design. This skill adds the checks and the
traps.

## 1. Make the targets

1. Build the counters d8 of the change-based fuzzing section of TUTORIAL.md,
   with `sanitizer_coverage_flags="inline-8bit-counters,pc-table"` and
   `symbol_level=1`.
2. Run `tools/change_targets.py` with the counters d8. For a measurement, always
   give `--base` and `--head`. Without them, the script compares your working
   tree with its merge base with origin/main, so your own changes become
   targets.
3. The script runs `git` in the checkout that contains it. Give `--repo=DIR`
   if the commits are only in another checkout.
4. Read the last line of the output. It gives the changed lines, the line and
   function targets, the excluded fatal-path edges and the focus.

Rules:

- Make the targets file from the same d8 that the session runs. Redline
  compares only the number of edges, not the build ID.
- Make all targets files of one campaign with the same version and options of
  `change_targets.py`. For example, a file with a focus and a file without a
  focus give different sessions.
- The script writes the edge map to `redline/edges.jsonl.gz` next to d8. Keep
  that directory writable.
- The script needs `llvm-objdump` in `third_party/llvm-build/Release+Asserts/bin/`,
  or `--objdump=PATH`. Without it, the script keeps the edges that only lead to
  a fatal error, and it shows 0 excluded edges.
- `(capped)` in the last line means that the targets reached `--max-fraction`
  of the code edges.
- With a bits d8, `redline fuzz --targets` stops with an error.

## 2. Read the session

- The start line shows `coverage counters: ... N target edges, M focus edges`.
- The status line shows `focus found/total` when the targets file has a focus.
  It shows `target found/total (line found/total)` when the file has targets.
  If the `target` field is missing, the session has no targets.
- The `stats` events of `events.jsonl` have `target_found` and `target_total`.
- `opt` counts the optimized code features. The field is missing when d8 runs
  without `--fuzzilli-enable-optimized-code-coverage`. It stays at 0 when the
  preset does not optimize code.

## 3. No line target

Examine these causes in this order:

1. The diff changes no `.cc`, `.h` or `.inl` file under `src/` or `include/`.
   The script ignores the other files, for example Torque files.
2. The code is not in d8. For example, the CSA builtin generators
   (`src/builtins/*-gen.cc`) run only in mksnapshot.
3. The d8 has no line tables. Build it with `symbol_level=1`.
4. The d8 does not come from the head revision of the diff.
5. The build does not compile the code, for example code for another
   architecture.

`unmapped_lines` in the targets file lists the changed lines that have no edge.

## 4. Targets that no run reaches

Examine these causes:

1. The fatal-path filter did not run (section 1).
2. A new flag that is off turns on the change. The `flags` field of the
   targets file names it, but Redline does not read this field. For the
   session of a CL, add the flag with `--d8-extra-args`. Never do this in a
   benchmark.
3. The preset does not run the tier of the change. For example, a Maglev
   change needs the flags of the `maglev` preset.
4. The function targets can go up while the line targets stay at 0. Read
   `line found/total`.

To find the cause:

- Write a small program that runs the changed code and ends without an error.
  Run it with `redline exec --d8=D8 --print-edges FILE`. Compare its edges with
  the `edge` values of the targets file. `exec` prints no edges for a program
  that crashes.
- Replay the saved corpus of a finished run on the counters d8 with
  `redline replay --d8=D8 --corpus=RUN/corpus --dump-edges=FILE`. Give the
  same d8 flags as the run, and `--fuzzilli-enable-optimized-code-coverage`.
  Then look for the `edge` of each target in `FILE`. This also compares the
  corpora of normal mode and change-based fuzzing on one build.
- In the targets file, a line target has a `w` of 1 (`--line-weight`). A
  function target has a smaller `w` (`--function-weight`, default 0.3).

## 5. Weights and focus

- `--target-boost=N` (default 20) sets the extra weight of a target edge. The
  `coverage` block of a configuration sets the same values.
- By default, Redline drops a new program when all its new edges are outside
  the focus. `{"coverage": {"drop_unfocused": false}}` keeps such programs.
- The target-guided d8 flags (the `flags.target_*` tunables) work only with
  `randomize_d8_flags`. The restart of a d8 that reaches no target also needs
  it.
- Tune the weights only on the training bundles of the
  [redline-bug-reinjection](../redline-bug-reinjection/SKILL.md) skill.
  Measure a new default on the held-out bundles before you change it.

## 6. Measure change-based fuzzing against normal mode

Use the bug bundles of the
[redline-bug-reinjection](../redline-bug-reinjection/SKILL.md) skill and the
scripts of the [redline-ab-testing](../redline-ab-testing/SKILL.md) skill.

- Run normal mode on the bits d8 and change-based fuzzing on the counters d8
  of the same bundle. Give both arms the same seeds, flags, workers and time.
- Decide if the arms draw random d8 flags. Without random flags, change-based
  fuzzing loses its target-guided flags. Give both arms the same rule, and
  write the rule in the report.
- The counters build is slower. To separate the effect of the targets from
  the cost of the build, add a normal mode arm on the counters d8. Then it
  differs from the `redline-change` arm only by `--targets`:

  ```shell
  PRESET=$(cat $BB_ROOT/FIX/preset) \
  EXTRA_FLAGS=--fuzzilli-enable-optimized-code-coverage \
    experimental/redline/skills/redline-ab-testing/scripts/run_arm.sh \
    redline-normal $BB_ROOT/FIX/counters/d8 OUT DURATION SEED
  ```

- If change-based fuzzing loses on a bug, also measure it with
  `drop_unfocused: false` on the whole bug set.
- Compare the finds and the time to the find. Do not compare the coverage of
  the counters build with the coverage of the bits build.
- Before a campaign, the builder of each bundle checks that a short session
  finds a target (redline-bug-reinjection section 5). Do not use a bundle
  without a found target to measure change-based fuzzing.
