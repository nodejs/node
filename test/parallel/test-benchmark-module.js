'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('module', [
  'n=1',
  'useCache=true',
  'fullPath=true'
]);
