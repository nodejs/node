'use strict';
const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  size: [16, 512, 1024, 4096, 16386],
  millions: [1]
});

function main(conf) {
  const iter = (conf.millions >>> 0) * 1e6;
  const size = (conf.size >>> 0);
  const b0 = new Buffer(size).fill('a');
  const b1 = new Buffer(size).fill('a');

  b1[size - 1] = 'b'.charCodeAt(0);

  // Force optimization before starting the benchmark
  b0.compare(b1);
  v8.setFlagsFromString('--allow_natives_syntax');
  eval('%OptimizeFunctionOnNextCall(b0.compare)');
  b0.compare(b1);

  bench.start();
  for (var i = 0; i < iter; i++) {
    b0.compare(b1);
  }
  bench.end(iter / 1e6);
}
