Use the `minimize-reproducer` skill to minimize `tmp/poc.js` (a Maglev fuzzer
crash) into a clean regression test written to `tmp/poc-min.js`. It crashes with
`Check failed: !v8::internal::v8_flags.enable_slow_asserts.value() || (IsDereferenceAllowed())`
when run with
`bash tmp/repro.sh --fuzzing --jit-fuzzing --future --no-lazy-feedback-allocation --allow-natives-syntax tmp/poc.js`.
Strip dead noise, unused computed properties, dummy classes/arrays, obfuscated
names, and umbrella fuzzer flags, and report the minimal required flags and
final lines.
