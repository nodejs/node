'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

// Because the dgram benchmarks use hardcoded ports, this should be in
// sequential rather than parallel to make sure it does not conflict with
// tests that choose random available ports.

runBenchmark('dgram', ['address=true',
                       'chunks=2',
                       'dur=0.1',
                       'len=1',
                       'n=1',
                       'num=1',
                       'type=send']);
