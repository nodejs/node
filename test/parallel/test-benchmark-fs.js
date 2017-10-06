'use strict';

require('../common');
const runBenchmark = require('../common/benchmark');

runBenchmark('fs', [
  'n=1',
  'size=1',
  'dur=1',
  'len=1024',
  'concurrent=1',
  'pathType=relative',
  'statType=fstat',
  'statSyncType=fstatSync',
  'encodingType=buf',
  'filesize=1024'
]);
