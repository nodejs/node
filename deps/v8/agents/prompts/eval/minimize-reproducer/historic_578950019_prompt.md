Use the `minimize-reproducer` skill to minimize `tmp/poc.js` (a
`MaterializedObjectStore` array left-trim bug) into a clean regression test
written to `tmp/poc-min.js`. It crashes with
`Debug check failed: stale filler pointer in MaterializedObjectStore` when run
with
`bash tmp/repro.sh --fuzzing --jit-fuzzing --future --expose-gc --allow-natives-syntax tmp/poc.js`.
Unwrap `eval(code)`, strip dead noise and umbrella fuzzer flags, preserve the
minimum required array length (`> 100`) and minimal flags, and report the
minimal required flags and final lines.
