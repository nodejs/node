'use strict';
const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  pair: ['a', 'a=b'],
  pairs: [1e3, 1e5],
  n: [100],
});

function main({ pair, pairs, n }) {
  const input = `${pair}&`.repeat(pairs);
  let params;
  bench.start();
  for (let i = 0; i < n; i++) {
    params = new URLSearchParams(input);
  }
  bench.end(n);
  assert.strictEqual(params.size, pairs);
}
