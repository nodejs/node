'use strict';

require('../common');

const runBenchmark = require('../common/benchmarks');

runBenchmark('arrays', ['n=1', 'type=Array']);
