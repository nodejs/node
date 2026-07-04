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

The generated cases span the interesting character-width axes: ASCII, Latin-1,
BMP two-byte, and supplementary (surrogate-pair) code points.

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

Reproduce or minimize a single known case instead of fuzzing:

```sh
tools/regexp/correctness_fuzzer/correctness_fuzzer.py --ref A --test B \
    --pattern '(?=|)()x|' --flags '' --subject z
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
| `--pattern` | | Reproduce a single case (with `--flags` / `--subject`). |

The exit status is non-zero when any divergence is found.

## Files

- `correctness_fuzzer.py` -- case generation, execution, diff, and minimization.
- `harness.js` -- the `d8` harness that constructs and runs each regexp.
