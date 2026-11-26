Use the `minimize-reproducer` skill to minimize `tmp/poc.js` (a partial
script-cache crash) into a clean regression test written to `tmp/poc-min.js`. It
crashes with `Debug check failed: pos.line > 0` when run with
`bash tmp/repro.sh --fuzzing --stress-background-compile --expose-gc --bytecode-old-time=0 --cpu-profiler tmp/poc.js`.
Strip dead helpers, redundant wrappers, obfuscated names, and unnecessary flags,
and report the minimal required flags and final lines.
