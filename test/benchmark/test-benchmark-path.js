'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('path', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
