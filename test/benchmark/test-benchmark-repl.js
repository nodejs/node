'use strict';

const common = require('../common');
const runBenchmark = require('../common/benchmark');

common.skipIfInspectorDisabled();

runBenchmark('repl', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
