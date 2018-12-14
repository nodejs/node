'use strict';
const common = require('../common.js');
const { randomBytes } = require('crypto');

const KB = 1024;
const MB = KB * KB;

const bench = common.createBenchmark(main, {
  size: [10, KB, 2 * KB, 8 * KB, 64 * KB, 256 * KB, MB, 8 * MB, 32 * MB],
  n: [1024]
});

const buf = randomBytes(32 * MB * 2);

function main({ n, size }) {
  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    buf.copy(buf, size, 0, size);
  }
  bench.end(n);
}
