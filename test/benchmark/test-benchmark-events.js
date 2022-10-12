'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('events', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
