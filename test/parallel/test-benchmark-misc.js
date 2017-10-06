'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('misc', [
  'n=1',
  'val=magyarország.icom.museum',
  'millions=.000001',
  'type=extend',
  'concat=0'
]);
