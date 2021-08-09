'use strict';
/* global SharedArrayBuffer */

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1e7]
});

function main({ n }) {
  if (typeof SharedArrayBuffer === 'undefined') {
    throw new Error('SharedArrayBuffers must be enabled to run this benchmark');
  }

  const i32arr = new Int32Array(new SharedArrayBuffer(4));
  bench.start();
  for (let i = 0; i < n; i++)
    Atomics.wait(i32arr, 0, 1);  // Will return immediately.
  bench.end(n);
}
