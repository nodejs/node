'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('util', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
