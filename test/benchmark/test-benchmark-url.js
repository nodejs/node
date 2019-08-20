'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('url',
             [
               'method=legacy',
               'e=0',
               'loopMethod=forEach',
               'accessMethod=get',
               'type=short',
               'searchParam=noencode',
               'href=short',
               'input=short',
               'domain=empty',
               'path=up',
               'to=ascii',
               'prop=href',
               'n=1',
               'param=one',
               'withBase=false'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
