'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for HTTP benchmark test');

// Because the http benchmarks use hardcoded ports, this should be in sequential
// rather than parallel to make sure it does not conflict with tests that choose
// random available ports.

const runBenchmark = require('../common/benchmark');

runBenchmark('http',
             [
               'benchmarker=test-double',
               'c=1',
               'chunkedEnc=true',
               'chunks=0',
               'dur=0.1',
               'key=""',
               'len=1',
               'method=write',
               'n=1',
               'res=normal',
               'type=asc'
             ],
             {
               NODEJS_BENCHMARK_ZERO_ALLOWED: 1,
               duration: 0
             });
