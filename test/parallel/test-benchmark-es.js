'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('es',
             [
               'method=',
               'millions=0.000001',
               'count=1',
               'context=null',
               'rest=0',
               'mode=',
               'n=1',
               'encoding=ascii',
               'size=1e1'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
