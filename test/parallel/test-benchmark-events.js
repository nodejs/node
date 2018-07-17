'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('events',
             ['argc=0', 'staleEventsCount=0', 'listenerCount=1', 'n=1'],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
