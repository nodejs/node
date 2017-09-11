'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('zlib',
             [
               'method=deflate',
               'n=1',
               'options=true',
               'type=Deflate'
             ]);
