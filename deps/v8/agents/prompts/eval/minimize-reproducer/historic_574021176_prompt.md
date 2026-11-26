Use the `minimize-reproducer` skill to minimize `tmp/poc.js` (a Maglev generator
`CLASS_SCOPE` bug) into a clean regression test written to `tmp/poc-min.js`. It
crashes with `Debug check failed: exposed ContextCell via leaked CLASS_SCOPE`
when run with
`bash tmp/repro.sh --fuzzing --jit-fuzzing --future --allow-natives-syntax tmp/poc.js`.
Strip dead noise and redundant wrapper helpers, replace the 2000-iteration
warmup loop with `%PrepareFunctionForOptimization` and
`%OptimizeFunctionOnNextCall`, strip umbrella fuzzer flags, and report the
minimal required flags and final lines.
