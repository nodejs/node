'use strict';

require('../common');

// Minimal test for assert benchmarks. This makes sure the benchmarks aren't
// completely broken but nothing more than that.

const runBenchmark = require('../common/benchmark');

runBenchmark(
  'assert',
  [
    'strict=1',
    'len=1',
    'method=',
    'n=1',
    'primitive=null',
    'size=1',
    'type=Int8Array'
  ]
);
