'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('es', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
