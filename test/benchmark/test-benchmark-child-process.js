'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('child_process', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
