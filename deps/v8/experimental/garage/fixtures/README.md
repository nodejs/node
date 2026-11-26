# Fixture workloads

JS workloads used by [`tests/golden.rs`](../tests/golden.rs) (which generates
traces on the fly in memory from `d8` during test runs) and by
[`tools/gen-fixtures.sh`](../tools/gen-fixtures.sh) when saving a local corpus
to disk. Generated `.log` files are git-ignored so multi-megabyte traces are not
checked into the V8 repository.

```
workloads/          JS inputs (see below)
large/              multi-hundred-MB perf trace (git-ignored, see tools/gen-large-trace.sh)
```

## Files are ANSI-colored and that is deliberate

d8 does not check `isatty`, so a piped trace carries SGR escapes mid-line. The
fixtures preserve them byte for byte, because that is what the parser and the
raw view have to cope with. Use `sed 's/\x1b\[[0-9;]*m//g'` when eyeballing.

## Workloads

| File | Targets |
| :-- | :-- |
| `simple.js` | smallest Maglev graph; the baseline parser fixture |
| `deopt-eager.js` | wrong-map eager deopt, then reoptimization (two instances of one function) |
| `osr.js` | OSR compilation with a bytecode offset |
| `inlining.js` | inlining decisions, including one rejected callee |
| `poly.js` | monomorphic → polymorphic → megamorphic feedback |
| `truncation.js` | float64 arithmetic behind `\| 0`; the truncation phases |
| `gc.js` | allocation churn for `--trace-gc` |
| `mixed.js` | several functions tiering at different times, one flip-flopping |
| `throw.js` | exception handlers: both `↳ throw` deopt-frame forms (see [spike-findings.md](../docs/spike-findings.md) §12) |

## Naming

`<flag-slug>.<workload>.log`, e.g. `maglev-graphs+inlining.inlining.log` is
`--print-maglev-graphs --trace-maglev-inlining` on `inlining.js`. The full
matrix is defined in `tests/golden.rs` and `tools/gen-fixtures.sh`.

## Reproducibility

`tools/gen-fixtures.sh` runs every command twice and records whether the two
runs are byte-identical in the local `manifest.json` it writes. About half the
fixtures per build are stable; the rest carry wall-clock timings, native
pointers, PIDs or thread interleaving (see
[../docs/spike-findings.md](../docs/spike-findings.md) §10 for what varies and
why).

## Generating locally

`garage_golden_tests` generates traces in memory automatically from `d8`. To
save a local corpus to disk for manual inspection:

```bash
tools/gen-fixtures.sh /path/to/out/x64.release/d8
```
