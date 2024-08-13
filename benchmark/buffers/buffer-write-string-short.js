'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  len: [0, 1, 8, 16, 32],
  fallback: [1, 0],
  n: [1e6],
});

function main({ len, n, fallback }) {
  const buf = Buffer.allocUnsafe(len);
  const string = fallback && len > 0
    ? Buffer.from('a'.repeat(len - 1) + '€').toString()
    : Buffer.from('a'.repeat(len)).toString()
  bench.start();
  for (let i = 0; i < n; ++i) {
    buf.write(string);
  }
  bench.end(n);
}
