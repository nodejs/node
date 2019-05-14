'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('vm',
             [
               'breakOnSigint=0',
               'withSigintListener=0',
               'n=1'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
