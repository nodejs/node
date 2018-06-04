'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for TLS benchmark test');

// Because the TLS benchmarks use hardcoded ports, this should be in sequential
// rather than parallel to make sure it does not conflict with tests that choose
// random available ports.

const runBenchmark = require('../common/benchmark');

runBenchmark('tls',
             [
               'concurrency=1',
               'dur=0.1',
               'n=1',
               'size=2',
               'securing=SecurePair',
               'type=asc'
             ],
             {
               NODEJS_BENCHMARK_ZERO_ALLOWED: 1,
               duration: 0
             });
