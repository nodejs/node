'use strict';
const common = require('../common.js');
const {
  Blob,
} = require('node:buffer');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [50e3],
});

let _cloneResult;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    _cloneResult = structuredClone(new Blob(['hello']));
  bench.end(n);

  // Avoid V8 deadcode (elimination)
  assert.ok(_cloneResult);
}
