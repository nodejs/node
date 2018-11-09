'use strict';

require('../common');
const runBenchmark = require('../common/benchmark');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

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
  'filesize=1024',
  'dir=.github',
  'withFileTypes=false'
], { NODE_TMPDIR: tmpdir.path, NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
