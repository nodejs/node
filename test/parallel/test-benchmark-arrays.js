'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('arrays', ['n=1', 'type=Array']);
