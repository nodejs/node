'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('zlib',
             [
               'method=deflate',
               'n=1',
               'options=true',
               'type=Deflate',
               'inputLen=1024',
               'duration=0.1'
             ]);
