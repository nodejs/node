'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for green thread benchmark test');

const runBenchmark = require('../common/benchmark');

runBenchmark('green_threads', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
