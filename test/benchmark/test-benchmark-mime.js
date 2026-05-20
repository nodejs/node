'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('mime', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
