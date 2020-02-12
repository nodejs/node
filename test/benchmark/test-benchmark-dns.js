'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('dns', { ...process.env, NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
