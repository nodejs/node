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
               'benchmarker=test-double-http',
               'c=1',
               'e=0',
               'url=long',
               'arg=string',
               'chunkedEnc=true',
               'chunks=0',
               'dur=0.1',
               'duplicates=1',
               'input=keep-alive',
               'key=""',
               'len=1',
               'method=write',
               'n=1',
               'res=normal',
               'type=asc',
               'value=X-Powered-By'
             ],
             {
               NODEJS_BENCHMARK_ZERO_ALLOWED: 1,
               duration: 0
             });
