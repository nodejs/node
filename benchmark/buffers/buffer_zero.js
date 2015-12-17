'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1024]
});

const zero = new Buffer(0);

function main(conf) {
  var n = +conf.n;
  bench.start();
  for (let i = 0; i < n * 1024; i++) {
    new Buffer(zero);
  }
  bench.end(n);
}
