'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('esm', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
