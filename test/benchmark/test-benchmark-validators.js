'use strict';

require('../common');

// Minimal test for assert benchmarks. This makes sure the benchmarks aren't
// completely broken but nothing more than that.
const runBenchmark = require('../common/benchmark');

runBenchmark('validators');
