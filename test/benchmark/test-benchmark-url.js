'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('url', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
