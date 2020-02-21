'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('zlib',
             [
               'algorithm=brotli',
               'chunkLen=1024',
               'duration=0.001',
               'inputLen=1024',
               'method=',
               'n=1',
               'options=true',
               'type=Deflate',
             ],
             {
               'NODEJS_BENCHMARK_ZERO_ALLOWED': 1
             });
