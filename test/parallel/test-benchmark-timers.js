'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('timers',
             [
               'type=depth',
               'n=1'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
