'use strict';

require('../common');

const runBenchmark = require('../common/benchmark');

runBenchmark('cluster', ['n=1', 'payload=string', 'sendsPerBroadcast=1']);
