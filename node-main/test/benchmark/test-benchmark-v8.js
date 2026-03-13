'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('v8', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
