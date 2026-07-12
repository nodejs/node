'use strict';

const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  len: [256, 1024 * 16],
  n: [1e5],
});

function main({ n, len }) {
  const typedArray = new BigUint64Array(len);
  bench.start();
  for (let i = 0; i < n; i++)
    v8.serialize({ a: 1, b: typedArray });
  bench.end(n);
}
