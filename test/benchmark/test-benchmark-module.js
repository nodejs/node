'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('module', [
  'cache=true',
  'dir=rel',
  'ext=',
  'fullPath=true',
  'n=1',
  'name=/',
  'useCache=true',
]);
