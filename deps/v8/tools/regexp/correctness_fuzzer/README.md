# Regexp correctness fuzzer

A differential correctness fuzzer for V8's regexp engine. It generates random
patterns, flags, and subject strings, runs each through two `d8` configurations,
and reports any case where the observed result (match, captures, `index`,
`lastIndex`) differs, or where one configuration crashes. Every divergence is
delta-minimized before it is printed.

A two-config diff catches both wrong-answer miscompiles and crashes that a
single run would miss, exercising regexp shapes that mjsunit and the recorded
benchmark corpus do not cover exhaustively. The two configurations are
arbitrary: a build under test against a stock reference, the interpreter against
the JIT, or two flag settings.

Patterns come from a weighted grammar transcribed from ECMA-262 22.2.1
(`grammar/rules.py`), one rule per grammar alternative. The spec's own parameters --
`[UnicodeMode]`, `[UnicodeSetsMode]`, `[NamedCaptureGroups]` -- are threaded
through the derivation, so mode-specific syntax (`\q{}`, `&&`, `\p{}`) is only
emitted in a mode that accepts it and generated patterns are essentially always
syntactically valid. Cases span the interesting character-width axes -- ASCII,
Latin-1, BMP two-byte, supplementary -- and subjects are drawn from the
characters the pattern itself mentions, so roughly half of all cases match
rather than failing at the first character.

Each case runs on two separately constructed `RegExp` objects, and both results
are reported. The two differ in what the engine has cached by the time they
run: a pattern is compiled by its first `exec`, and whatever that compile
leaves on the `RegExpData` (generated code, quick-check mask, first-character
filter) reaches the second object through the compilation cache. So the first
`exec` runs a cold pattern and the second a warm one, along paths a
single-shot run would never take.

Reporting both also makes each case self-checking. The two objects are
identical and start from the same `lastIndex`, so their results must agree
whatever either configuration computes -- a mismatch is reported as
`COLD/WARM` and is a bug in that configuration by itself. That is the one bug
class a two-config diff cannot see, since it survives being wrong on both
sides.

## Usage

Compare a build under test against a stock reference build:

```sh
tools/regexp/correctness_fuzzer/correctness_fuzzer.py \
    --ref   ~/stock/out/x64.release/d8 \
    --test  out/x64.release/d8
```

Or compare two flag configurations of a single binary, with no second build:

```sh
tools/regexp/correctness_fuzzer/correctness_fuzzer.py \
    --ref   "out/x64.release/d8:--regexp-interpret-all" \
    --test  "out/x64.release/d8"
```

The reference is treated as ground truth: only cases the reference executes
cleanly are compared, so an unsupported-syntax difference in the reference is
never reported as a test failure. A syntax error is a clean result; a hard
abort (`CHECK`/`DCHECK` failure or segfault) is not, and is reported as a
crash whenever it occurs on either side, independent of any divergence. Point
both configurations at a `dcheck_always_on` build to surface those.

Findings come in three kinds:

| Kind | Meaning |
| --- | --- |
| `CRASH` | Either configuration aborted. A bug regardless of what the other did. |
| `COLD/WARM` | One configuration disagreed with itself across the two execs. A bug in that configuration alone; needs no reference. |
| `DIVERGENCE` | Both ran cleanly and the test disagreed with the reference. |

Aim a campaign at a family of patterns with `--profile`, or tune one grammar
rule directly:

```sh
tools/regexp/correctness_fuzzer/correctness_fuzzer.py --ref A --test B \
    --profile classes --coverage

tools/regexp/correctness_fuzzer/correctness_fuzzer.py --ref A --test B \
    --weight Assertion.caret=8 --weight Atom.pattern_character=0.5
```

`--coverage` reports how often each grammar rule fired and, more usefully,
which rules never fired -- a rule at zero means the run tested nothing about
that construct, which case volume alone will not reveal.

Reproduce or minimize a single known case instead of fuzzing:

```sh
tools/regexp/correctness_fuzzer/correctness_fuzzer.py --ref A --test B \
    --pattern '(?=|)()x|' --flags '' --subject z
```

Or replay a whole generated testcase file -- typically one ClusterFuzz attached
to a bug (see below) -- classifying every case in it and minimizing each
finding:

```sh
tools/regexp/correctness_fuzzer/correctness_fuzzer.py --ref A --test B \
    --testcase fuzz-3.js
```

## Options

| Flag | Default | Meaning |
| --- | --- | --- |
| `--ref` | (required) | Reference `d8` (ground truth), `path` or `path:--flags`. |
| `--test` | (required) | `d8` under test, `path` or `path:--flags`. |
| `--seed` | random | Base seed; batch `i` uses a deterministic derived seed. Randomized and printed when omitted. |
| `--batches` | `100` | Number of batches to run. |
| `--batch-size` | `500` | Cases per batch. |
| `--max-findings` | `20` | Stop after this many divergences. |
| `--progress-every` | `25` | Print a flushed progress line every N clean batches (`0` disables). |
| `--profile` | `default` | Weight overlay: `anchored`, `classes`, `backtracking`, `loops`, `lookaround`. |
| `--weight` | | Multiply one rule's weight, `Production.rule=N`; repeatable. |
| `--max-depth` | `5` | Derivation depth budget. The default is the smallest that reaches every grammar rule. |
| `--coverage` | off | Report per-rule expansion counts and unexercised rules. |
| `--pattern` | | Reproduce a single case (with `--flags` / `--subject` / `--last-index`). |
| `--testcase` | | Replay a generated testcase file (`run.py` output, e.g. a ClusterFuzz `fuzz-N.js`). |

The exit status is non-zero when any divergence is found.

Output is flushed as it is produced, so a long run stopped with `Ctrl-C`
keeps everything printed so far even when redirected to a file: the seed, every
finding, and the progress trail. An interrupt during a finding's minimization
still prints that finding unminimized before exiting.

## ClusterFuzz

The same generator runs on ClusterFuzz as a blackbox fuzzer, with foozzie
(`tools/clusterfuzz/foozzie/`) as the application. `run.py` implements the
blackbox contract -- `--input_dir`, `--output_dir`, `--no_of_files` -- and only
generates: each `fuzz-N.js` is a self-contained script of ~500 cases plus the
harness, which foozzie executes under two of its configurations and diffs.
Everything downstream (crash detection, dedup, minimization, bisection, bug
filing) is ClusterFuzz's. Foozzie already ships in the build archive, so no new
application or build target is needed.

Both entry points emit their batches through `harness.py`, so a ClusterFuzz
testcase is byte-for-byte what the interactive tool runs; `--testcase` takes one
back apart and hands each case to the real minimizer. Relative to the
interactive tool, the ClusterFuzz side gains the cross-architecture comparisons
(x64 against the arm/arm64 simulators). ClusterFuzz only removes complete case
lines during minimization.

Each case prints `v8-foozzie source: regexp-fuzzer:<tag>`. Foozzie hashes the
last label before a difference into a `regexp-<hash>` key. `harness.py`
derives the tag from pattern metacharacters to group similar patterns,
independent of V8 output.

Simulate a job locally before uploading; this is also how to judge how noisy a
grammar change is under foozzie's rules, which unlike the interactive tool have
no ground-truth side to skip a configuration-dependent exception:

```sh
tools/regexp/correctness_fuzzer/run_locally_against_foozzie.py \
    --d8 out/x64.release/d8 --files 100
```

Which configurations foozzie compares is chosen by the fuzzer, as for
js_fuzzer: next to each `fuzz-N.js`, `run.py` writes a `flags-N.js` with the
`v8_foozzie.py` flags for it (`--first-config`, `--second-config`,
`--second-d8`, `--second-config-extra-flags`, and a fixed `--random-seed`).
It draws them from
`tools/clusterfuzz/foozzie/v8_fuzz_experiments_regexp.json` and
`v8_fuzz_flags_regexp.json`, read from the build directory the fuzzer runs
against (`$APP_DIR` on ClusterFuzz, where the V8 build copies them next to d8;
the foozzie source directory when run from a checkout).

Each experiment row is `[probability, baseline, second, second_d8]`. Foozzie
always runs the baseline, an implicit flagless `default` on `second_d8`, and
`second` on `second_d8`, and diffs the latter two against the baseline. The
regexp table uses `jitless` (implies `--regexp-interpret-all`, the bytecode
interpreter) as the baseline, so every file compares the x64 interpreter
against native code on x64 or the arm64/arm/ia32 simulators through the
implicit `default` run. `second` is `slow_path` (`--force-slow-path`, the
runtime `RegExpExec` path) throughout, since a `default` second config would
duplicate the implicit run. x64 gets the largest share as the fastest and
least flaky configuration. The flag table holds regexp codegen toggles that
must be observationally invisible, plus `--regexp-tier-up-ticks=1` to exercise
the interpreter-to-native tier-up mid-run; they are applied to the `default`
and `second` runs.

A generated case with exponential backtracking occasionally exceeds foozzie's
per-run timeout; foozzie treats that as a pass and the file is lost, which at
the observed rate (about 1 in 100 files) is noise. The dry run counts and
keeps such files.

Build the bundle with `package.sh` (default output `out/regexp_fuzzer.zip`):
`run.py`, `harness.py`, `harness.js`, `grammar/` and a copy of js_fuzzer's
`foozzie_launcher.py`, all at the top level. It is uploaded through the
ClusterFuzz UI and attached to the existing foozzie correctness jobs and plain
d8 sanitizer jobs; re-run and re-upload whenever the grammar or harness
changes.

## Tests

```sh
tools/regexp/correctness_fuzzer/grammar_test.py --d8 out/x64.release/d8
```

Pins the properties that decay silently: every grammar rule stays reachable
under every profile, mode-specific syntax stays in its mode, generated patterns
parse, a meaningful share of cases match, and the emitted testcase format
stands alone, survives a line-based minimizer, and round-trips through
`--testcase`. Omit `--d8` for the fast presubmit checks with bounded samples.
Use `--exhaustive` for full Python samples and rule/profile coverage without
a build. `--d8` also enables the exhaustive Python checks.

## Files

- `grammar/` -- the ECMA-262 pattern grammar.
  - `registry.py` -- the `@rule` decorator, `Context`, and `expand()`.
  - `menus.py` -- character pools and generation constants.
  - `rules.py` -- the grammar itself, one `@rule` per spec alternative.
  - `cases.py` -- flags, subjects, and whole cases.
  - `profiles.py` -- weight overlays and `--weight` parsing.
- `grammar_test.py` -- tests for the above, the harness, and the testcase format.
- `correctness_fuzzer.py` -- interactive entry point: execution, diff, and minimization.
- `harness.js` -- the `d8` harness that constructs and runs each regexp.
- `harness.py` -- emits cases plus harness as one self-contained script, and parses it back.
- `run.py` -- ClusterFuzz entry point: generates testcase files and their foozzie flags.
- `run_locally_against_foozzie.py` -- local simulation of a ClusterFuzz foozzie job.
- `package.sh` -- builds the zip uploaded to ClusterFuzz.

A rule never makes a weighted choice of its own: anything that would be an
`if rng.random() < p` inside a rule body is a separate `@rule` with its own
weight instead, since a branch is a weight that neither `--weight` nor
`--coverage` can see. A test pins this.
