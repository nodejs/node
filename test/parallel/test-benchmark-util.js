'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('util',
             ['argument=false',
              'input=',
              'method=Array',
              'n=1',
              'option=none',
              'type=',
              'version=native'],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
