'use strict';
const common = require('../common.js');
const { Blob } = require('node:buffer');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [50e3],
  dataSize: [
    5,
    30 * 1024,
  ],
});

let _sliceResult;

function main({ n, dataSize }) {
  const data = 'a'.repeat(dataSize);
  const reusableBlob = new Blob([data]);

  bench.start();
  for (let i = 0; i < n; ++i)
    _sliceResult = reusableBlob.slice(0, Math.floor(data.length / 2));
  bench.end(n);

  // Avoid V8 deadcode (elimination)
  assert.ok(_sliceResult);
}
