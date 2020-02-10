'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('util',
             ['argument=false',
              'input=',
              'method=Array',
              'n=1',
              'option=none',
              'pos=start',
              'size=1',
              'type=',
              'len=1',
              'version=native',
              'isProxy=1',
              'showProxy=1'],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
