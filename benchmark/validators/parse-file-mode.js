'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  value: [
    "'777'",
    '0o777',
  ],
}, {
  flags: ['--expose-internals'],
});

function getParseFactory() {
  const {
    parseFileMode,
  } = require('internal/validators');

  return (n) => parseFileMode(n, 'n');
}

function main({ n, value }) {
  const parse = getParseFactory();

  value = value === "'777'" ? '777' : 0o777;

  // Warm up.
  const length = 1024;
  const array = [];
  let errCase = false;

  for (let i = 0; i < length; ++i) {
    try {
      array.push(parse(value));
    } catch (e) {
      errCase = true;
      array.push(e);
    }
  }

  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    try {
      array[index] = parse(value);
    } catch (e) {
      array[index] = e;
    }
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], errCase ? 'object' : 'number');
  }
}
