'use strict';

const runBenchmark = require('../common/benchmarks');

runBenchmark('cluster', ['n=1', 'payload=string', 'sendsPerBroadcast=1']);
