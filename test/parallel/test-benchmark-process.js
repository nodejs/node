'use strict';

const runBenchmark = require('../common/benchmarks');

runBenchmark('process',
             [
               'millions=0.000001',
               'n=1',
               'type=raw'
             ]);
