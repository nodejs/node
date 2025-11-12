'use strict';
const common = require('../common');

const bench = common.createBenchmark(main, {
  len: [2, 16, 256], // x16
  n: [4e6],
});

function main({ n, len }) {
  const data = Buffer.alloc(len * 16, 'a');
  const expected = Buffer.byteLength(data, 'buffer');
  let changed = false;
  bench.start();
  for (let i = 0; i < n; i++) {
    const actual = Buffer.byteLength(data, 'buffer');
    if (expected !== actual) { changed = true; }
  }
  bench.end(n);
  if (changed) {
    throw new Error('Result changed during iteration');
  }
}
