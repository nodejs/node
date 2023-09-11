'use strict';

require('../common');

// Because the net benchmarks use hardcoded ports, this should be in sequential
// rather than parallel to make sure it does not conflict with tests that choose
// random available ports.

const runBenchmark = require('../common/benchmark');

runBenchmark('net', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
