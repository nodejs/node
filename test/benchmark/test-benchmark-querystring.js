'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('querystring', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
