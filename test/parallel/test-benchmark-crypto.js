'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasFipsCrypto)
  common.skip('some benchmarks are FIPS-incompatible');

const runBenchmark = require('../common/benchmark');

runBenchmark('crypto',
             [
               'algo=sha256',
               'api=stream',
               'cipher=',
               'keylen=1024',
               'len=1',
               'n=1',
               'out=buffer',
               'type=buf',
               'v=crypto',
               'writes=1',
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
