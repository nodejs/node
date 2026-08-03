'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  value: [
    '[]',
    '[1,2,3]',
  ],
}, {
  flags: ['--expose-internals'],
});

function getValidateFactory() {
  const {
    validateArray,
  } = require('internal/validators');

  return (n) => validateArray(n, 'n');
}

function main({ n, value }) {
  const validate = getValidateFactory();

  switch (value) {
    case '[]':
      value = [];
      break;
    case '[1,2,3]':
      value = [1, 2, 3];
      break;
  }

  // Warm up.
  const length = 1024;
  const array = [];
  let errCase = false;

  for (let i = 0; i < length; ++i) {
    try {
      array.push(validate(value));
    } catch (e) {
      errCase = true;
      array.push(e);
    }
  }

  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    try {
      array[index] = validate(value);
    } catch (e) {
      array[index] = e;
    }
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], errCase ? 'object' : 'undefined');
  }
}
