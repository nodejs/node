'use strict';
const common = require('../common.js');
const { randomFillSync } = require('crypto');

const bench = common.createBenchmark(main, {
  size: [10, 1024, 2048, 8192, 64 * 1024, 256 * 1024, 1024 * 1024, 8 * 1024 * 1024, 32 * 1024 * 1024],
  n: [1024]
});

function main({ n, size }) {
  const buf = Buffer.allocUnsafe(size * 2);
  randomFillSync(buf);

  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    buf.copy(buf, size, 0, size);
  }
  bench.end(n);
}
