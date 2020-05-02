'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  bytes: [0, 8, 128, 32 * 1024],
  partial: ['true', 'false'],
  oob: ['true', 'false'],
  n: [6e6]
});

function main({ n, bytes, partial, oob }) {
  const source = Buffer.allocUnsafe(bytes);
  const target = Buffer.allocUnsafe(bytes * 2);
  const sourceStart = (partial === 'true' ? Math.floor(bytes / 2) : 0);
  const sourceEnd = (oob === 'true' ? source.length + 1 : source.length);
  bench.start();
  for (let i = 0; i < n; i++) {
    source.copy(target, 0, sourceStart, sourceEnd);
  }
  bench.end(n);
}
