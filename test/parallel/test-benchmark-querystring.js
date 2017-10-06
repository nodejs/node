'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('querystring', ['n=1', 'input=', 'type=']);
