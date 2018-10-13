'use strict';

const common = require('../common');

if (common.isWindows) {
  common.skip('vcbuild.bat doesn\'t build the n-api benchmarks yet');
}

const runBenchmark = require('../common/benchmark');

runBenchmark('napi',
             [
               'n=1',
               'engine=v8',
               'type=String'
             ],
             { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
