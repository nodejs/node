'use strict';

const common = require('../common');

if (common.isWindows) {
  common.skip('vcbuild.bat doesn\'t build the Node.js API benchmarks yet');
}

if (!common.isMainThread) {
  common.skip('addons are not supported in workers');
}

if (process.features.debug) {
  common.skip('benchmark does not work with debug build yet');
}
const runBenchmark = require('../common/benchmark');

runBenchmark('node-api', { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
