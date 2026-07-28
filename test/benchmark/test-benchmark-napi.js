'use strict';

const common = require('../common');

if (common.isWindows) {
  common.skip('vcbuild.bat doesn\'t build the Node-API benchmarks yet');
}

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('addons are not supported in workers');
}

if (common.isDebug) {
  common.skip('benchmark does not work with debug build yet');
}
const runBenchmark = require('../common/benchmark');

runBenchmark('napi', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
