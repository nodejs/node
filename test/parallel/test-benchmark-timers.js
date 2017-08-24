'use strict';

const runBenchmark = require('../common/benchmarks');

runBenchmark('timers',
             [
               'type=depth',
               'millions=0.000001',
               'thousands=0.001'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
