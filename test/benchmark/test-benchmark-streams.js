'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('streams',
             [
               'kind=duplex',
               'n=1',
               'sync=no',
               'type=buffer',
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
