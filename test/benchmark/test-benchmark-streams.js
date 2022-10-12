'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('streams', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
