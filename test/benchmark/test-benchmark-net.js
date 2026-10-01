'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('net', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
