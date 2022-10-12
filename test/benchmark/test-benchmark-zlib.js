'use strict';

require('../common');

const runBenchmark = require('./benchmark');

runBenchmark('zlib', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
