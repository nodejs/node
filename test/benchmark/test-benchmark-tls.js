'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for TLS benchmark test');

const runBenchmark = require('../common/benchmark');

runBenchmark('tls', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
