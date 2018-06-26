'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('path',
             [
               'n=1',
               'path=',
               'pathext=',
               'paths=',
               'props='
             ], { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
