'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for Worker benchmark test');

// Because the worker benchmarks can run on different threads,
// this should be in sequential rather than parallel to make sure
// it does not conflict with tests that choose random available ports.

const runBenchmark = require('../common/benchmark');

runBenchmark('worker',
             [
               'n=1',
               'sendsPerBroadcast=1',
               'workers=1',
               'payload=string'
             ],
             {
               NODEJS_BENCHMARK_ZERO_ALLOWED: 1
             });
