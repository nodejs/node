# ImportDefer benchmark

Synthetic throughput benchmark for the **import defer** proposal
(`import defer * as ns from "..."`), enabled by the `--js-defer-import-eval`
flag.

The `HotAccess` arms read a module namespace's exports in a tight loop. Each
scenario ships an **eager control** (`import * as`) and a **deferred arm**
(`import defer * as`) that differ only in the import keyword, so the comparison
isolates the per-access cost of a deferred namespace once its module has been
evaluated. After the first access evaluates the module, repeated reads of a
deferred namespace should be as fast as reads of an ordinary namespace.

## Running locally

Run from **inside this directory** — relative specifiers resolve relative to the
process cwd, and the perf harness (`tools/run_perf.py`) already runs d8 with the
suite path as cwd:

```
cd test/js-perf-test/ImportDefer
../../../out/arm64.release/d8 --allow-natives-syntax --js-defer-import-eval run.js
```

The fixtures self-check (`m.value !== 190 * iterations`) and throw on failure, so
a clean run that prints the expected score lines is a pass.

`run.js` reports ops/sec (`units: "score"`, **higher is better**); it measures
steady-state per-access cost. Absolute numbers are machine- and run-dependent;
the signal is the **eager-vs-deferred comparison within a single run**.

## Files

```
run.js                 BenchmarkSuite throughput driver (HotAccess arms)
value.js               mutable-export module read by the HotAccess arms
hotaccess-eager.js     eager control: import * as
hotaccess-defer.js     deferred arm:  import defer * as
```

## Perf-harness registration

This suite has its own standalone config, `test/js-perf-test/ImportDefer.json`
(deliberately not part of the bulk `JSTests1..5.json` grouping, mirroring
`ClassFields.json`, `RegExp.json`, etc.), so it only runs when a runner is
pointed at it explicitly:

```
tools/run_perf.py --arch arm64 --binary-override-path out/arm64.release/d8 \
  test/js-perf-test/ImportDefer.json
```
