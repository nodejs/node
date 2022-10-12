'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('timers', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
