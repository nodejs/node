'use strict';

require('../common');
const runBenchmark = require('../common/benchmark');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

runBenchmark('fs', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
