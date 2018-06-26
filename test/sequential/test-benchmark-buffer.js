'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('buffers',
             [
               'aligned=true',
               'args=1',
               'buffer=fast',
               'byteLength=1',
               'charsPerLine=6',
               'encoding=utf8',
               'endian=BE',
               'len=2',
               'linesCount=1',
               'method=',
               'n=1',
               'pieces=1',
               'pieceSize=1',
               'search=@',
               'size=1',
               'source=array',
               'type=',
               'value=0',
               'withTotalLength=0'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
