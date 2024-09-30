'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['slice', 'subarray'],
  n: [1e6],
});

const buf = Buffer.allocUnsafe(1024);

function main({ n, type }) {
  const b = buf
  const fn = type === 'subarray' ?
    () => b.subarray(10, 256) :
    () => b.slice(10, 256);

  bench.start();
  for (let i = 0; i < n; i++) {
    fn();
  }
  bench.end(n);
}
