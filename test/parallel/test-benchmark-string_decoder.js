'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('string_decoder', ['chunk=16',
                                'encoding=utf8',
                                'inlen=32',
                                'n=1']);
