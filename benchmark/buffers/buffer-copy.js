'use strict';
const common = require('../common.js');
const { randomBytes } = require('crypto');

const KB = 1024;

const bench = common.createBenchmark(main, {
  size: [10, KB, 2 * KB, 8 * KB, 64 * KB /* , 256 * KB */],
  // This option checks edge case when target.length !== source.length.
  targetStart: [0, 1],
  n: [1024]
});

function main({ n, size, targetStart }) {
  const source = randomBytes(size);
  const target = randomBytes(size);

  bench.start();
  for (var i = 0; i < n * 1024; i++) {
    source.copy(target, targetStart);
  }
  bench.end(n);
}
