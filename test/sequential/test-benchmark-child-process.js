'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('child_process',
             [
               'dur=0',
               'n=1',
               'len=1',
               'params=1',
               'methodName=execSync',
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
