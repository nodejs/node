'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  arg: [true, false],
  len: [0, 1, 64, 1024],
  n: [1e7]
});

function main(conf) {
  const arg = conf.arg;
  const len = conf.len | 0;
  const n = conf.n | 0;
  const buf = Buffer(len).fill(42);

  var i;
  bench.start();
  if (arg) {
    for (i = 0; i < n; i += 1)
      buf.toString('utf8');
  } else {
    for (i = 0; i < n; i += 1)
      buf.toString();
  }
  bench.end(n);
}
