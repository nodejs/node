'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

const env = Object.assign({}, process.env,
                          { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });

runBenchmark('dns', ['n=1', 'all=false', 'name=127.0.0.1'], env);
