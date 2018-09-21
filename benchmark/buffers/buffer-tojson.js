'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e4],
  len: [0, 10, 256, 4 * 1024]
});

function main({ n, len }) {
  const buf = Buffer.allocUnsafe(len);

  bench.start();
  for (var i = 0; i < n; ++i)
    buf.toJSON();
  bench.end(n);
}
