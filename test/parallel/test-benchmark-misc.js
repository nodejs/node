'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('misc', [
  'concat=0',
  'method=',
  'n=1',
  'type=extend',
  'val=magyarország.icom.museum'
], { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
