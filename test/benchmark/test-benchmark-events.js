'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('events', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
