'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('timers',
             [
               'type=depth',
               'n=1',
               'millions=0.000001',
               'thousands=0.001',
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
