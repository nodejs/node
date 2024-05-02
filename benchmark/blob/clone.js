'use strict';
const common = require('../common.js');
const {
  Blob,
} = require('node:buffer');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [50e3],
  bytes: [128, 1024, 1024 ** 2],
});

let _cloneResult;

function main({ n, bytes }) {
  const buff = Buffer.allocUnsafe(bytes);
  const blob = new Blob(buff);
  bench.start();
  for (let i = 0; i < n; ++i)
    _cloneResult = structuredClone(blob);
  bench.end(n);

  // Avoid V8 deadcode (elimination)
  assert.ok(_cloneResult);
}
