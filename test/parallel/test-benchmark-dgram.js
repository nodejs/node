'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('dgram', ['address=true',
                       'chunks=2',
                       'dur=0.1',
                       'len=1',
                       'n=1',
                       'num=1',
                       'type=send']);
