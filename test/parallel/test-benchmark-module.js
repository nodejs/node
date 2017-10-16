'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('module', [
  'thousands=.001',
  'useCache=true',
  'fullPath=true'
]);
