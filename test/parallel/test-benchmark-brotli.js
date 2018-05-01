// Flags: --expose-brotli
'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('brotli',
             [
               'method=compress',
               'n=1',
               'options=true',
               'type=Compress',
               'quality=4',
             ]);
