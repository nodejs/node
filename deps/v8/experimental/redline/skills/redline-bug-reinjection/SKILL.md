---
name: redline-bug-reinjection
description: Builds d8 binaries that contain a known V8 bug again, to measure how Redline and Fuzzilli find bugs. Covers the choice of fixes, the training and held-out sets, the revert and the two d8 builds. Also covers the change-based fuzzing targets, the check of the crash and the scoring of a find. Use it before a bug-finding measurement of Redline. Do not use it to design generators.
---

# Re-inject known bugs

A re-injected bug is a fixed V8 bug that you put back. You revert its fix on a
recent origin/main and build d8 from the revert. A fuzzer finds the bug when it
makes a program that crashes with the signature of the bug.

Keep the bug lists, the builds and the results outside the V8 checkout.
[AGENTS.md](../../AGENTS.md) section 3.2 tells why.

## 1. Choose the fixes

1. List recent fixes in the area, for example:

   ```shell
   git log --since=4.months --format='%h %cs %s' origin/main -- src/maglev src/compiler/turboshaft
   ```

2. Keep a fix only if all of these are true:
   - The fix has a regression test, usually in `test/mjsunit/`.
   - The test crashes a fuzzing d8 without the fix: a failed `CHECK` or
     `DCHECK`, a fatal error or a signal. A wrong result without a crash needs
     a differential oracle, which Redline does not have.
   - The fix reverts cleanly on today's origin/main.
3. Prefer fixes that a fuzzer found. They show what a fuzzer can reach.

## 2. Split the training set and the held-out set

- The **training set** can guide work on the engine, for example the weights
  of change-based fuzzing. Do not design generators from it either (AGENTS.md
  section 3.4).
- The **held-out set** measures if a change generalizes. Decide which fixes go
  into it before anybody reads them.
- Only the person or agent that builds a held-out bug reads its test, its fix
  and its d8 flags. Others never read them. Give them only the bug ID and the
  preset. Do not run `git show` or `git log` on the fix of a held-out bug.
- Give the crash signature only to the scoring step. A signature names the
  file, the function and the check of the bug.
- Only the builder of the held-out set compares the random flag list of
  Redline with the `d8_args` of the held-out bugs. If a flag is in both, use
  fixed flags for that bug, or remove the flag from the list. Name each overlap
  in the report.
- Replace the held-out set when work starts to use it. Fixes that landed after
  the last change to the generators make a good new set.

## 3. Build the bundle

A bug bundle is one directory for each fix. The private files go to another
directory, which only the builder and the scoring step read:

```text
$BB_ROOT/FIX/bits/d8           # with snapshot_blob.bin and icudtl.dat
$BB_ROOT/FIX/counters/d8       # with snapshot_blob.bin and icudtl.dat
$BB_ROOT/FIX/fuzzilli/d8       # with snapshot_blob.bin and icudtl.dat
$BB_ROOT/FIX/targets.json      # the change-based fuzzing targets, section 4
$BB_ROOT/FIX/preset            # maglev, turbolev, ...
$BB_ROOT/FIX/fix               # the commit of the fix
$BB_ROOT/FIX/revision          # the commit of the revert
$BB_PRIVATE/FIX/signature      # a regular expression, section 5
$BB_PRIVATE/FIX/d8_args        # the flags of the regression test, section 5
$BB_PRIVATE/FIX/test.js        # the regression test, section 5
```

The three d8 binaries come from the same revision: the base revision with the
revert of the fix. Use the arguments of TUTORIAL.md section 1 for all three,
and change only the coverage flags. The d8 of Fuzzilli uses `v8_fuzzilli=true`
instead of `v8_redline=true`:

| Build | `sanitizer_coverage_flags` | Use |
|---|---|---|
| bits | `"inline-bool-flag"` | normal mode of Redline |
| counters | `"inline-8bit-counters,pc-table"`, with `symbol_level=1` | change-based fuzzing of Redline (the change-based fuzzing section of TUTORIAL.md) |
| fuzzilli | `"inline-bool-flag"`, with `v8_fuzzilli=true` | Fuzzilli |

### 3.1 Prepare the bundle checkout

`BB_V8` is a second V8 checkout, only for bundles. The script checks out other
revisions in it, so your work checkout does not change.

1. Make `BB_V8` with its own gclient solution, for example
   `mkdir bb && cd bb && fetch v8`. A `git worktree` has no gclient
   dependencies.
2. Make the three build directories with `gn gen`: `out/fuzzbuild` with the
   bits arguments, `out/rl_counters` with the counters arguments and
   `out/fuzzilli` with the arguments of the d8 of Fuzzilli.
3. Before each set of bundles, run `git fetch origin`,
   `git checkout --detach origin/main` and `gclient sync` in `BB_V8`.

### 3.2 Run the script

[scripts/make_bundle.sh](scripts/make_bundle.sh) makes one bundle:

```shell
BB_ROOT=$BB_ROOT BB_V8=$BB_V8 \
  experimental/redline/skills/redline-bug-reinjection/scripts/make_bundle.sh FIX maglev
```

- The base revision (`BASE`, default origin/main) must have the coverage
  counters of d8 in `experimental/redline/d8/cov.cc`. The script stops if it
  does not.
- The script commits the revert without a branch and tags it `bb/FIX`.
  Branches are not necessary.
- To build the same bundle again, run the script with `BASE=bb/FIX^`.
- Exit status 3 means that the revert does not apply. Choose another fix.
- A bundle takes about 1 GB. On some workstations `/tmp` is in memory. Keep the
  bundles on a disk.

## 4. Make the change-based fuzzing targets

The targets are the edges of the code that the revert puts back.
`make_bundle.sh` writes them with the counters build. To make them again:

```shell
python3 experimental/redline/tools/change_targets.py --repo=$BB_V8 \
    --d8=$BB_ROOT/FIX/counters/d8 --base=bb/FIX^ --head=bb/FIX \
    --objdump=$BB_V8/third_party/llvm-build/Release+Asserts/bin/llvm-objdump \
    --out=$BB_ROOT/FIX/targets.json
```

Read the last line of the output. A bundle without line targets cannot
measure change-based fuzzing. The
[redline-change-fuzzing](../redline-change-fuzzing/SKILL.md) skill lists the
causes.

## 5. Check the bundle and write the signature

Only the person or agent that builds the bundle does this step.

1. The revert removes the regression test from `bb/FIX`. Copy the test from
   the fix commit to the private directory:
   `git -C $BB_V8 show FIX:PATH > $BB_PRIVATE/FIX/test.js`.
2. Write the flags of the `// Flags:` line of the test to
   `$BB_PRIVATE/FIX/d8_args`.
3. From the root of `BB_V8`, run the test on both builds. Add the
   `d8_extra_args` of the preset:

   ```shell
   $BB_ROOT/FIX/bits/d8 --fuzzing --allow-natives-syntax \
       $(cat $BB_PRIVATE/FIX/d8_args) PRESET_FLAGS \
       test/mjsunit/mjsunit.js $BB_PRIVATE/FIX/test.js
   ```

4. Both builds must crash. An error about a missing file is not a result. Fix
   the command and run it again. If a build does not crash, drop the fix.
5. Write a regular expression that matches this crash and no other crash. Use
   the file and line of the failed check, or the functions of a stack overflow.
6. Make sure that the expression matches the two forms of the crash:

   | Tool | Text that the expression must match | Example |
   |---|---|---|
   | Redline | the `signature` of the `crash` event | `fatal foo.cc:123 IsSmi(value).` |
   | Fuzzilli | the d8 output in the `STDERR:` part of the crash file | `# Fatal error in ../../src/foo.cc, line 123` |

   For example, `foo\.cc(:|, line )123` matches both. A Fuzzilli crash file
   also contains the program and the d8 flags. Do not write an expression that
   matches a flag name.
7. Write the expression to `$BB_PRIVATE/FIX/signature`.
8. Never give `d8_args` to a fuzzer. The fuzzer must find the bug with the
   flags of its preset or its own random flags.
9. Run `redline selftest --d8=D8` on both builds. The line of the
   `DCHECK failure` crash case must say `crashes`. Without DCHECKs, a fuzzer
   cannot find a `DCHECK` bug.
10. Start a change-based fuzzing session of 10 minutes on the counters d8.
    Import the tests of the changed area, for example `test/mjsunit/maglev` for
    a fix in `src/maglev/`. The first number of `target N/M` in the status line
    must go above 0. If it stays at 0, do not use the bundle to measure
    change-based fuzzing.

## 6. Score a run

- A run finds the bug at the time of its first crash that matches the
  signature and reproduces.
- Redline runs each crash again in a fresh d8. In the `crash` event of
  `events.jsonl`, `reproducible` is true if a rerun crashes with any
  signature. Count a find only if `same_signature` is 1 or more.
- Fuzzilli puts `deterministic` or `flaky` in the name of each crash file.
  Count only `deterministic` files. The time of a find is the time of the file
  minus the start time of the run.
- Fuzzilli checks only that the program crashes again, not the signature. For
  a strict comparison, run the first matching crash file of each Fuzzilli run
  again on the bundle d8.
- A crash that does not reproduce is not a find. Report it as a note.
- An out-of-memory abort is not a find.
- With random d8 flags, read `d8_random_args` in the `crash` event of a find.
  Tell if the find needs one of these flags.
- `score_runs.py` of the [redline-ab-testing](../redline-ab-testing/SKILL.md)
  skill applies these rules. Give it
  `--signature-file=$BB_PRIVATE/FIX/signature`.

## 7. Known problems

- A d8 can stay alive when its fuzzer dies. Look for old d8 processes after
  each campaign and stop the ones that your runs started.
- Some fixes need a flag that no preset sets. Do not add that flag to a preset
  for the benchmark. Note the bug as "needs flag" instead.
- A revert can also remove a test or a helper that other code uses. Build
  before you trust a bundle.
- A bundle can show other crashes. Run an unknown crash on the d8 of the base
  revision. A crash on both builds is an older bug. A crash only on the bundle
  can be another symptom of the injected bug. Write it as a note, and do not
  count it as a find.
- To replay a state-dependent crash `NAME.js`, give `redline exec --d8=D8` the
  files of `NAME.history/` in their order, and then `NAME.js`. `exec` applies
  no preset, so give `--d8-extra-args` with the preset flags and the
  `d8_random_args` of the crash event. `exec` runs each file as a separate
  script in one d8 process. Plain d8 runs many files in one global scope, so
  their declarations collide.
