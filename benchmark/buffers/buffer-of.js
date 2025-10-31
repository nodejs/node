'use strict';

const common = require('../common.js');

// Measure Buffer.of(...items) throughput for various lengths.
// We prebuild the items array to avoid measuring array construction,
// and vary the effective iterations with length to keep total work reasonable.

const bench = common.createBenchmark(main, {
  len: [0, 1, 8, 64, 256, 1024],
  n: [5e5],
});

function main({ len, n }) {
  const items = new Array(len);
  for (let i = 0; i < len; i++) items[i] = i & 0xFF;

  bench.start();
  for (let i = 0; i < n; i++) {
    Buffer.of(...items);
  }
  bench.end(n);
}
