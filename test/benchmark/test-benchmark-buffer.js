'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('buffers', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
