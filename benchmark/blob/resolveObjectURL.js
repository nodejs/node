'use strict';
const common = require('../common.js');
const { Blob, resolveObjectURL } = require('node:buffer');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [50e3],
});

let _resolveObjectURL;

function main({ n }) {
  const blob = new Blob(['hello']);
  const id = URL.createObjectURL(blob);
  bench.start();
  for (let i = 0; i < n; ++i)
    _resolveObjectURL = resolveObjectURL(id);
  bench.end(n);

  // Avoid V8 deadcode (elimination)
  assert.ok(_resolveObjectURL);
}
