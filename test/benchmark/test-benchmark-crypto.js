'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasFipsCrypto)
  common.skip('some benchmarks are FIPS-incompatible');

if (common.hasOpenSSL3) {
  common.skip('temporarily skipping for OpenSSL 3.0-alpha15');
}

const runBenchmark = require('../common/benchmark');

runBenchmark('crypto', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
