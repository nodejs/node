'use strict';

require('../common');
const runBenchmark = require('../common/benchmark');

runBenchmark('fs', [
  'n=1',
  'size=1',
  'dur=1',
  'filesize=1024'
]);
