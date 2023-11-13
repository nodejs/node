'use strict';

const common = require('../common');
if (!common.enoughTestMem)
  common.skip('Insufficient memory for Websocket benchmark test');

// Because the websocket benchmarks use hardcoded ports, this should be in sequential
// rather than parallel to make sure it does not conflict with tests that choose
// random available ports.

const runBenchmark = require('../common/benchmark');

runBenchmark('websocket', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
