'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e6]
});


function main(conf) {
  const n = conf.n >>> 0;

  bench.start();
  for (var i = 0; i < n; i++) {
    process.hrtime();
  }
  bench.end(n);
}
