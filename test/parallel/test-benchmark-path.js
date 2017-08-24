'use strict';

const runBenchmark = require('../common/benchmarks');

runBenchmark('path',
             [
               'n=1',
               'path=',
               'pathext=',
               'paths=',
               'props='
             ]);
