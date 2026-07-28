'use strict';

const { skip } = require('../common');

if (process.config.variables.node_shared_ada) {
  skip('WHATWG URL parsing is affected by different versions of Ada');
}

const runBenchmark = require('../common/benchmark');

runBenchmark('url', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
