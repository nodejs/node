'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  value: [
    'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
    'UPPERCASE',
    'lowercase',
    'mixedCase',
  ],
}, {
  flags: ['--expose-internals'],
});

function main({ n, value }) {

  const toASCIILower = require('internal/mime').toASCIILower;
  // Warm up.
  const length = 1024;
  const array = [];
  let errCase = false;

  for (let i = 0; i < length; ++i) {
    try {
      array.push(toASCIILower(value));
    } catch (e) {
      errCase = true;
      array.push(e);
    }
  }

  // console.log(`errCase: ${errCase}`);
  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    try {
      array[index] = toASCIILower(value);
    } catch (e) {
      array[index] = e;
    }
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], errCase ? 'object' : 'string');
  }
}
