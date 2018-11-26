'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('misc', [
  'concat=0',
  'dur=0.1',
  'method=',
  'n=1',
  'type=',
  'val=magyarország.icom.museum',
  'script=test/fixtures/semicolon',
  'mode=worker'
], { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
