'use strict';

// Entry point of a startup snapshot that starts the pprof time profiler, which
// must throw: see test/parallel/test-snapshot-perf-hooks-pprof.js.
const { time } = require('node:perf_hooks/pprof');
time.start();
