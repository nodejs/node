'use strict';
const common = require('../common.js');
const assert = require('node:assert');
const { Blob } = require('buffer');

const bench = common.createBenchmark(main, {
  n: [1e6],
});

function main({ n }) {
  const blob = new Blob(['hello']);
  let id = '';
  bench.start();
  for (let i = 0; i < n; i += 1) {
    id = URL.createObjectURL(blob);
    URL.revokeObjectURL(id);
  }
  assert.ok(id);
  bench.end(n);
}
