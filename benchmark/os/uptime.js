'use strict';

const os = require('os');
const common = require('../common.js');
const assert = require('assert');

const uptime = os.uptime;

const bench = common.createBenchmark(main, {
  n: [1e5],
});

function main({ n }) {
  if (os.type() === 'OS400') {
    console.log('Skipping: os.uptime is not implemented on IBMi');
    process.exit(0);
  }

  // Warm up.
  const length = 1024;
  const array = [];
  for (let i = 0; i < length; ++i) {
    array.push(uptime());
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    const index = i % length;
    array[index] = uptime();
  }
  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], 'number');
  }
}
