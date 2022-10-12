'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('process', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
