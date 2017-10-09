'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('dgram', ['dur=0.1', 'chunks=2']);
