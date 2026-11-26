Use the `minimize-reproducer` skill to minimize `tmp/poc.js`, which crashes with
`DCHECK failed: target_invariant()` when run with
`bash tmp/fake_d8_semantic.sh --fuzzing --jit-fuzzing --future --expose-gc tmp/poc.js`.
Write a minimal, clean, self-contained reproducer to `tmp/poc-min.js` and report
the minimal required flags and final lines.
