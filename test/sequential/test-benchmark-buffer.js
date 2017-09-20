'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('buffers',
             [
               'aligned=true',
               'args=1',
               'encoding=utf8',
               'len=2',
               'method=',
               'n=1',
               'noAssert=true',
               'pieces=1',
               'pieceSize=1',
               'search=@',
               'size=1',
               'source=array',
               'type=',
               'withTotalLength=0'

             ]);
