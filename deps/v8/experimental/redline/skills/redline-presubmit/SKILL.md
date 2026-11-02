---
name: redline-presubmit
description: Runs and reports the checks that a change to experimental/redline must pass before upload. The checks cover formatting, lint, the build, the selftests, the parser round trip and the golden traces. They also cover the Markdown links, the ASD-STE100 language of the text, changed generators and mutators, and a before/after fuzzing comparison. Use it before you upload, or approve, a CL that changes experimental/redline. Do not use it for changes outside experimental/redline.
---

# Redline presubmit

Use this skill before you upload a change to `experimental/redline/`. It turns
the checklist of [CONTRIBUTE.md](../../CONTRIBUTE.md) section 11 into commands,
and it gives a report for the CL description.

Run all commands from the root of the V8 checkout, with depot_tools in your
`PATH`. The scripts are in `experimental/redline/skills/redline-presubmit/scripts/`.

## 1. Prepare

1. Build the fuzzing d8 in `out/fuzzbuild` and Redline in `out/redline`.
   [TUTORIAL.md](../../TUTORIAL.md) sections 1 and 2 give the commands.
2. Commit your change. The scripts compare `HEAD` with a base revision. By
   default, the base revision is the merge base with the upstream branch.

## 2. Run the standard checks

```shell
experimental/redline/skills/redline-presubmit/scripts/check.sh
```

The script prints one line for each check:

| Check | Pass criterion |
|---|---|
| `git cl format` | No file needs formatting. |
| `git cl lint` | 0 errors. |
| build | `autoninja -C out/redline redline` succeeds. `BUILD.gn` makes every warning an error. |
| `selftest --offline` | All checks pass. |
| `selftest --d8` | All checks pass against `out/fuzzbuild/d8`. |
| `roundtrip --no-run` | 0 real failures on `test/mjsunit`. |
| golden traces | Information only: the number of changed files in `testdata/`. |
| Markdown links | `tools/dev/markdown_checker.py` accepts the changed Markdown files. |
| STE language | See step 4. |

The options are `--out=DIR`, `--d8=PATH` and `--base=REV`. A FAIL line gives
the cause or the path of its log. Fix every FAIL before you continue.

## 3. Check what your change touches

| You changed | Do this | Pass criterion |
|---|---|---|
| The golden traces in `testdata/` | Make sure that you want the new output. Then run `out/redline/redline selftest --offline --write-golden` and examine `git diff experimental/redline/testdata/`. | The CL description tells why the traces changed. |
| `fast/`: lexer, parser, printer or scopes | `check.sh` runs `roundtrip --no-run`. For a change to the printer or to the scope analysis, also run the round trip with execution, at `HEAD` and at the base revision: `out/redline/redline roundtrip --dir=test/mjsunit --d8=out/fuzzbuild/d8`. | No new failure. This run reports known behavior differences as failures, so compare the two failure lists. |
| A generator | `out/redline/redline mutate --generate --generator=NAME --n=500 --run --d8=out/fuzzbuild/d8 --quiet` (CONTRIBUTE.md sections 3.4 and 3.5). If you have a d8 with known bugs, also run the audit of section 3.7. | The summary line shows at least 99.8% V8-accepted. In the audit, no crash signature takes more than 0.5% of the programs. |
| A mutator | `out/redline/redline mutate --mutator=NAME --n=50 --run --d8=out/fuzzbuild/d8 --quiet FILE...` with some `test/mjsunit` files (CONTRIBUTE.md section 4.4). | The summary line shows at least 99.8% V8-accepted. |
| What a session does: generators, mutators, selection strategies, presets, `engine/`, `exec/` or `corpus/` | Step 5. | See step 5. |

## 4. Check the language

Write the text of a change in ASD-STE100 Simplified Technical English. This
includes the Markdown files, the commit message and the code comments. Run the
linter on the text of your change:

```shell
experimental/redline/skills/redline-presubmit/scripts/ste_check.py
```

| Text | Pass criterion |
|---|---|
| A Markdown file that the change adds or edits | No more hard findings than at the base revision. |
| The commit message of `HEAD` | No hard finding. |
| The comments that the change adds to C++, Python, shell and GN files | No hard finding. |

Advisory findings, for example passive voice, do not make the check fail. Use
`--verbose` to see them, and fix them if the text stays clear.

The linter is `ste_lint.py` in google3, so you need access to `/google/src`.
`ste_check.py` runs it with `vpython3` and
[scripts/ste_lint.vpython3](scripts/ste_lint.vpython3), which supplies
absl-py. Set `STE_LINT` to use another copy of the linter. To lint one more
file, for example a draft of a commit message:

```shell
vpython3 -vpython-spec experimental/redline/skills/redline-presubmit/scripts/ste_lint.vpython3 \
  /google/src/files/head/depot/google3/learning/gemini/agents/skills/communication/asd_ste100/ste_lint.py FILE
```

If you cannot use the linter, apply the main rules yourself: one instruction
in each sentence, at most 20 words in an instruction and 25 words in a
description, active voice, and the imperative in instructions.

## 5. Compare fuzzing before and after

Do this step if your change can change what a session does:

```shell
experimental/redline/skills/redline-presubmit/scripts/ab_run.sh --base=REV
```

REV is the revision before your change, for example `HEAD~1`.

- The script builds Redline at REV and at `HEAD` in `out/redline`. It copies
  each binary, with its presets and test data, to a work directory in `/tmp`.
- It runs the two sessions at the same time for one hour, with the flags of
  CONTRIBUTE.md section 10.1: `--preset=default`, 8 workers each, seed 42, and
  `--single-threaded --random-seed=42` for d8.
- It prints the coverage of both sessions every 5 minutes, and a summary.
- It checks out REV while it builds, so commit your change first. It then
  restores your branch.
- The two sessions start 16 d8 processes. On a workstation with fewer than 24
  cores, use `--jobs=4`.

The options are `--duration=SEC`, `--jobs=N`, `--work=DIR`, `--out=DIR` and
`--d8=PATH`. Flags after `--` go to both sessions.

Read the result as CONTRIBUTE.md section 10.2 tells:

- Two identical sessions differ by 0.25 to 0.35 percentage points. A
  difference below 0.5 points is probably noise.
- Compare at the same elapsed time first. Then compare the executions: a
  slower change must pay for its time.
- One run is weak evidence. If the difference is important, repeat the run.
- Compare the crash signatures. Explain each new signature.

## 6. Write the report

Add the results to the CL description, after the description of the change.
The commit message must also pass the STE check, so do not use semicolons:

```text
Tested:
  check.sh: all checks pass. The golden traces did not change.
  Generator <NAME>: 500 programs, <P>% V8-accepted.
  ab_run.sh --base=HEAD~1, 1 h, 8 workers each: coverage <A>% -> <B>%
  (<D> pp, probably noise), executions <R>x, no new crash signature.
```

If a check does not apply, write why. If you did not do a check, write that.
After the upload, start a CQ dry run: the presubmit bots also check the
formatting and the Markdown links.

## Known problems

- `tools/dev/markdown_checker.py` does not skip code blocks. It reads text in
  square brackets followed by text in parentheses as a link, also inside a
  code block. A C++ lambda with an empty capture list causes this. Write the
  example code in a different way.
- `ab_run.sh` stops if tracked files have changes, because it must check out
  the base revision. Commit or stash your changes first.
- `ste_check.py` exits with status 3 and `check.sh` shows SKIP if the linter is
  not available.
