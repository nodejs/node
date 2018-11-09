'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('timers',
             [
               'direction=start',
               'n=1',
               'type=depth',
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
