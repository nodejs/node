// Flags: --experimental-quic --no-warnings
'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('missing quic');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for QUIC benchmark test');

const runBenchmark = require('../common/benchmark');

runBenchmark('quic', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
