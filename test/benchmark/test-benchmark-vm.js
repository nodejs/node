'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('vm', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
