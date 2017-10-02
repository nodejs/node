'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('cluster', [
  'benchmarker=test-double',
  'n=1',
  'payload=string',
  'sendsPerBroadcast=1',
  'c=50',
  'len=16'
]);
