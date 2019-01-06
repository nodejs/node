'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('zlib',
             [
               'algorithm=brotli',
               'method=deflate',
               'n=1',
               'options=true',
               'type=Deflate',
               'inputLen=1024',
               'duration=0.001'
             ],
             {
               'NODEJS_BENCHMARK_ZERO_ALLOWED': 1
             });
