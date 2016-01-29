'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e5],
});


function main(conf) {
  const n = conf.n >>> 0;
  bench.start();
  for (var i = 0; i < n; i++) {
    // Access every item in object to process values.
    Object.keys(process.env);
  }
  bench.end(n);
}
