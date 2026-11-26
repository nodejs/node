Use the `minimize-reproducer` skill to minimize `tmp/poc.js`, which triggers
`DCHECK failed: target_invariant` when run with
`bash tmp/fake_d8.sh --fuzzing --jit-fuzzing --future --expose-gc tmp/poc.js`.
Write the minimal reproducer to `tmp/poc-min.js` and report the minimal required
flags and final line count.
