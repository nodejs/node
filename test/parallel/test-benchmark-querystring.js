'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('querystring',
             [ 'n=1',
               'input="there is nothing to unescape here"',
               'type=noencode'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
