'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('string_decoder', ['chunkLen=16',
                                'encoding=utf8',
                                'inLen=32',
                                'n=1']);
