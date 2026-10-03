'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for HTTP benchmark test');

const runBenchmark = require('../common/benchmark');

runBenchmark('http', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
