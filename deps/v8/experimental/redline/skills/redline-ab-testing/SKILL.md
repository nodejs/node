---
name: redline-ab-testing
description: Plans, runs, scores and reports fair comparisons of Redline with origin/main, with other Redline builds and with Fuzzilli. Covers the three measurements of Redline, which are bug finding on re-injected bugs, change-based fuzzing against normal mode, and coverage without seeds. Lists the setup rules, the operations, the statistics rules and the known traps. Use it before you claim that a change makes Redline better or worse. For a quick check of one change, use ab_run.sh of the redline-presubmit skill.
---

# Fair comparisons for Redline

Read CONTRIBUTE.md section 10 first. This skill adds the rules for long
campaigns, for comparisons with Fuzzilli and origin/main, and for bug finding.
Keep all results outside the V8 checkout ([AGENTS.md](../../AGENTS.md) section
3.2).

## 1. Choose the question

| Question | Arms | Target | Main number |
|---|---|---|---|
| Is the change better than origin/main? | origin/main, change | the coverage setup and the bug setup | coverage at equal time, finds and times |
| Does Redline beat Fuzzilli on coverage? | Redline, Fuzzilli | two fuzzing d8 of one revision | coverage at equal time |
| Does Redline find bugs that Fuzzilli misses? | Redline, Fuzzilli | re-injected bugs, bits builds | finds and time to find |
| Is change-based fuzzing faster than normal mode? | change-based fuzzing, normal mode | re-injected bugs | finds and time to find |

- Always add an origin/main arm when you measure a change. Without it, you
  cannot tell if the change helps.
- For an ablation, run a control arm of the same build at the same time. Do not
  compare an ablation with an older build or with another machine.
- The [redline-change-fuzzing](../redline-change-fuzzing/SKILL.md) skill has
  the extra rules for change-based fuzzing.

## 2. Setup rules

Give every arm of one comparison:

- the same d8 binary, or the same bug bundle. Fuzzilli needs its own d8: a
  build of the same revision with `v8_fuzzilli=true` instead of
  `v8_redline=true`, with the same other arguments,
- the same d8 flags,
- the same seeds, or no seeds for all arms,
- the same timeout, the same number of workers and the same duration,
- the same machine at the same time. Run the arms of one bug next to each
  other. Section 2.2 tells when you can use an earlier Fuzzilli arm again.

Give all arms of one repetition the same `--seed`. Give each repetition
another seed, for example 1000 plus the number of the repetition.

Typical setups:

| Setup | Target | d8 flags | Workers and time |
|---|---|---|---|
| Coverage | the fuzzing d8 of Redline, and the d8 of Fuzzilli | the flags of one preset, and `--random-seed=42 --single-threaded` | 30 workers, 400 ms timeout, 12 hours |
| Bug finding | each bundle of the set | the flags of the preset of the bundle | 4 or 5 workers for each arm, 400 ms timeout, 8 hours |
| Quick check of a change | `out/fuzzbuild/d8` | see `skills/redline-presubmit/scripts/ab_run.sh` | 8 workers, 1 hour |

The coverage setup and the bug setup use no seeds, `--prelude=none` and no
random d8 flags. `--prelude=none` removes the prelude of Redline (DESIGN.md
section 3), because Fuzzilli has no prelude.

### 2.1 Random d8 flags

- Compare Redline with Fuzzilli with fixed flags. This is the default of
  `run_arm.sh`.
- Redline (`randomize_d8_flags`) and Fuzzilli (`--argumentRandomization`) draw
  random flags from different lists. Random flags in both tools are not the
  same flags.
- If you measure random flags, set them in all arms (`RANDOM_FLAGS=1`), and
  name both flag lists in the report. The preset must set
  `randomize_d8_flags`, for example `maglev` or `turbolev`.

### 2.2 Fuzzilli

- Check that `FuzzilliCli --help` lists the `rarity` corpus scheduler. Use it,
  so that Redline does not beat a weak baseline. Give the Fuzzilli commit and
  its options in the report.
- Fuzzilli ignores `--minMutationsPerSample` with the `rarity` scheduler.
- Fuzzilli starts each worker after a random delay of 1 to 10 minutes, unless
  you set `--workerDelay`. `run_arm.sh` sets `--workerDelay=0`, so all workers
  start at once, as in Redline. Name the setting in the report.
- Set the timeout in each arm. Without `--timeout`, the V8 profile of Fuzzilli
  takes a timeout between 300 and 900 ms.
- Compare the base flags of `run_arm.sh` with `v8ProcessArgs` of your Fuzzilli
  build.
- Use an earlier Fuzzilli arm again only if the d8, the flags, the workers,
  the timeout and the machine type are the same. Tell this in the report.

### 2.3 Seeds

- Fuzzilli cannot import JavaScript seed files directly. Compare it with
  Redline without seeds. Or compile the seed files to FuzzIL with
  `FuzzILTool --compile`, and give Redline only the files that compiled.
- Before a run with seeds, run each seed file on each bundle d8 with the flags
  of the run. Remove the seeds that crash.
- Do not use a regression test of the bug set as a seed. Do not take seeds
  from a tree that is newer than the fix.

### 2.4 Check the setup before you score

- The `start` event of Redline has `imports`. A value above 0, or a jump of
  the coverage in the first `stats` event, shows seeds.
- Fuzzilli writes its d8 arguments to `settings.json`. Compare them with the
  command in `OUT.start` of the Redline arms.
- Compare the number of edges in the Fuzzilli log (`Initialized, N edges`)
  with `num_edges` in the `stats` events of Redline. The two numbers must be
  the same.
- Compare the `extra=` lines of all `OUT.start` files. All arms must have the
  same d8 flags.
- An older Redline build ignores a flag that it does not know, with the warning
  `unknown flag`. Look for this warning in the log of each arm.
- Change-based fuzzing needs the counters build. Its coverage is not
  comparable with the coverage of a bits build. Compare only finds and times.

## 3. Run the arms

### 3.1 Before you start

1. Copy each Redline build to its own directory, with the layout of a
   checkout: `DIR/out/redline/redline`, `snapshot_blob.bin` and `icudtl.dat`
   next to it, and `DIR/experimental/redline/presets` and `testdata`. Write
   the commit of the build to `DIR/COMMIT`. `run_arm.sh` copies it to
   `OUT.start`. Do not overwrite a binary that runs.
2. Use one presets directory for all arms (`PRESETS`, default: the presets of
   the checkout of `run_arm.sh`). Then all arms get the same d8 flags. If the
   change under test changes the d8 flags of a preset, give each arm the
   presets of its build, and write this in the report.
3. Name each run by the bundle, the arm and a new repetition number. Never use
   the name of a run that has data again. `run_arm.sh` stops if `OUT` exists.
4. Keep the total number of workers of all arms at or below the number of
   cores. Plan the memory for the peak of a long run, not for its start.
5. Keep the run outputs on a disk. On some workstations, `/tmp` is in memory.

### 3.2 Start the arms

[scripts/run_arm.sh](scripts/run_arm.sh) starts one arm with the rules of
section 2. The target is a bug bundle, or a d8 binary for the coverage setup.
Start each long run in its own session, so that it continues when your shell
ends:

```shell
S=experimental/redline/skills/redline-ab-testing/scripts
mkdir -p $RUNS
export REDLINE=$BUILDS/COMMIT/out/redline/redline FUZZILLI=/path/to/FuzzilliCli JOBS=5
# Bug finding: one run of each arm on one bundle.
setsid nohup $S/run_arm.sh redline-normal $BB_ROOT/FIX $RUNS/FIX-normal-1001 28800 1001 \
  > $RUNS/FIX-normal-1001.log 2>&1 < /dev/null &
setsid nohup $S/run_arm.sh redline-change $BB_ROOT/FIX $RUNS/FIX-change-1001 28800 1001 \
  > $RUNS/FIX-change-1001.log 2>&1 < /dev/null &
setsid nohup $S/run_arm.sh fuzzilli $BB_ROOT/FIX $RUNS/FIX-fuzzilli-1001 28800 \
  > $RUNS/FIX-fuzzilli-1001.log 2>&1 < /dev/null &
# Coverage: the same command for each tool. The fuzzilli arm gets the d8 of
# Fuzzilli (v8_fuzzilli=true) instead of out/fuzzbuild/d8.
JOBS=30 PRESET=turbolev EXTRA_FLAGS="--random-seed=42 --single-threaded" \
  setsid nohup $S/run_arm.sh redline-normal out/fuzzbuild/d8 $RUNS/cov-redline-1 43200 42 \
  > $RUNS/cov-redline-1.log 2>&1 < /dev/null &
```

- The arms are `redline-normal`, `redline-change` and `fuzzilli`.
  Change-based fuzzing needs a bundle.
- Every arm gets the base flags of Fuzzilli, the `d8_extra_args` of the preset
  and `EXTRA_FLAGS`.
- `FUZZILLI_ARGS` sets the options of Fuzzilli (default
  `--corpus=rarity --workerDelay=0`).
- The script writes the setup, the hashes of the binaries and the full command
  to `OUT.start`. It writes the exit status and the end time to `OUT.end`.
- Start Fuzzilli instances some seconds apart. Many instances that start at the
  same time can abort.

### 3.3 While the runs go

- After you start the arms, count the processes of each arm with `pgrep -fc`.
  Compare the count with your plan: look for missing, double and stopped runs.
- Look at the run logs, the memory of each fuzzer and the kernel log after one
  hour, and then some times each day.
- If an arm stops early, report it. Do not compare it with full runs.

### 3.4 When the runs end

- Each `events.jsonl` of Redline must end with an `end` event with the reason
  `stop`. `OUT.end` gives the exit status.
- For a run without this end, find the cause in the kernel log, for example
  with `dmesg -T | grep -E 'Killed process|segfault'`. Compare the time of the
  kernel line with the last `t` of the run.
- Each Fuzzilli log must have `Shutting down due to finished fuzzing`. If an
  instance does not stop, send `SIGINT` first. Send `SIGKILL` only after the
  final statistics are on the disk.

## 4. Score the runs

[scripts/score_runs.py](scripts/score_runs.py) reads Redline output
directories and Fuzzilli storage directories:

```shell
S=experimental/redline/skills/redline-ab-testing/scripts
# Bug finding: the signature comes from the private directory of the bundle.
python3 $S/score_runs.py --signature-file=$BB_PRIVATE/FIX/signature \
    --hours=1,4,8 $RUNS/FIX-*/
# Coverage: without a signature.
python3 $S/score_runs.py --hours=1,6,12 $RUNS/cov-*/
```

- For each run it prints the first find, the number of matching crashes, the
  coverage at each hour, the executions and the run time. A cell without a
  sample near its hour shows `-`.
- A find is a crash that matches the signature of the bundle and reproduces
  with the same signature (redline-bug-reinjection section 6).
- The script reads the file times of Fuzzilli. Copy run directories with
  `rsync -a`, `cp -a` or `tar`, which keep the file times.

Rules:

- Use the `coverage` field of the `stats` events. It leaves out the flaky edges
  that Redline suppresses. `found_edges` includes them.
- Before a firm coverage claim, replay the final corpus of each arm with
  `redline replay --determinism=3 --dump-edges=FILE`, on the same d8 with the
  same flags, timeout and `--max-execs-before-respawn`. Replay the `.js` files
  of the Fuzzilli corpus the same way. Then one rule counts the edges of both
  tools.
- Count crashes from the `crash` events. The `crashes/` directory has more than
  one file for each crash.
- Name the counter of each throughput number. Redline `executions` and
  Fuzzilli `totalExecs` count all d8 runs. Redline `fuzz_executions` and
  Fuzzilli `totalSamples` count only the new programs.

## 5. Statistics rules

- Run at least 3 repetitions of each arm for a claim. Give all values.
- A run that does not find the bug counts as a time longer than its duration.
  Give a median time only if more than half of the runs found the bug.
  Otherwise give the finds as k of n.
- Compare only runs of the same duration. If the durations differ, count only
  the finds before the end of the shortest run.
- If the times of two arms overlap and n is small, report no clear difference.
- Two identical coverage sessions differ by 0.25 to 0.35 points. A difference
  below 0.5 points is probably noise.
- Compare coverage at 1, 6 and 12 hours. A gain at 1 hour can disappear later.
  Do not keep a change because of a 1-hour result alone.
- Compare at equal elapsed time first. Equal executions give a second view. For
  late growth, also compare the arms at equal coverage, because an arm that is
  behind grows faster.
- Compare the executions of the first minutes too. A slow start delays the
  first crashes.
- A bug that no arm finds says nothing about the arms.
- Do not pool runs of different builds, machines or setups.

## 6. Report

- Give the setup, the commits, the number of runs and every value.
- Give the real end time of each run. Do not label a run with its planned
  duration.
- Give the bad results too: crashed runs, slower first crashes, lost bugs.
- Recompute each number from the raw data before you report it.
- If a build has a defect, mark all its results as withdrawn in your notes. Do
  not use them again.
- A CL description gets the result of that CL and its setup. Reports, tables of
  many bugs and plans stay in local notes.

## 7. Known traps

| Trap | Effect | Fix |
|---|---|---|
| Seeds in one arm only | Many finds come from the seed files | Same seeds or none |
| Random d8 flags in one arm only | The flags find the bug, not the generators | Same flag rule for all arms |
| `--d8-args` without `--d8-extra-args` | Redline loses the flags of the preset, because `--d8-args` replaces them | Give the preset flags again with `--d8-extra-args`, as `run_arm.sh` does |
| Change-based fuzzing coverage compared with bits coverage | A larger number that means nothing | Compare finds and times |
| A crash that does not reproduce with the same signature | A false find | Count only reproduced crashes |
| A run that stopped early | Its partial numbers look like full numbers | Check the `end` event and `OUT.end` of each run |
| A find in the first minutes, with many crashes of one signature | Maybe a generator that is a template of the bug | Run `redline audit` on that bundle before you report the find |
| Generators designed on a bug of the set | The bug set measures memory, not fuzzing | Use the held-out set (redline-bug-reinjection) |
| An output directory used again | Lost corpus and mixed data | A new name for each run |
| A copy that does not keep the file times | Wrong Fuzzilli times | `rsync -a`, `cp -a` or `tar` |
| A number copied from an old report | Wrong claims | Recompute from raw data |
