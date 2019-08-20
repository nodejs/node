'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('streams',
             [
               'kind=duplex',
               'type=buffer',
               'n=1'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
