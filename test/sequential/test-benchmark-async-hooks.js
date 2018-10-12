'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for TLS benchmark test');

const runBenchmark = require('../common/benchmark');

runBenchmark('async_hooks',
             [
               'method=trackingDisabled',
               'n=10'
             ],
             {});
