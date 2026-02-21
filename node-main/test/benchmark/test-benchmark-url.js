'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('url', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
