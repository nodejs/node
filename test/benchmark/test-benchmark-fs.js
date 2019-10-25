'use strict';

require('../common');
const runBenchmark = require('../common/benchmark');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

runBenchmark('fs', [
  'bufferSize=32',
  'concurrent=1',
  'dir=.github',
  'dur=0.1',
  'encodingType=buf',
  'filesize=1024',
  'len=1024',
  'mode=callback',
  'n=1',
  'pathType=relative',
  'size=1',
  'statSyncType=fstatSync',
  'statType=fstat',
  'withFileTypes=false',
], { NODE_TMPDIR: tmpdir.path, NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
