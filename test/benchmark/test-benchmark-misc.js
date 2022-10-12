'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('misc', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
