'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

// Dgram benchmarks use hardcoded ports. Thus, this test can not be run in
// parallel with tests that choose random ports.

runBenchmark('dgram');
