'use strict';

const common = require('../common');
const runBenchmark = require('../common/benchmark');

common.refreshTmpDir();

runBenchmark('fs', [
  'n=1',
  'size=1',
  'dur=0.1',
  'len=1024',
  'concurrent=1',
  'pathType=relative',
  'statType=fstat',
  'statSyncType=fstatSync',
  'encodingType=buf',
  'filesize=1024'
], { NODE_TMPDIR: common.tmpDir, NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
