'use strict';

const common = require('../common.js');
const os = require('os');

const bench = common.createBenchmark(main, {
  n: [100, 1000, 10000]
});

function main(conf) {
  var n = +conf.n;
  bench.start();
  for (let i = 0; i < n * 1024; i++) {
    os.tmpdir();
  }
  bench.end(n);
}
