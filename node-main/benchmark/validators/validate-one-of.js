'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  code: [
    'validateOneOf',
  ],
  value: [
    'fifo',
    'lifo',
    'lilo',
  ],
  validLength: [
    1,
    2,
    3,
  ],
}, {
  flags: ['--expose-internals'],
});

const validValues = [
  'fifo',
  'lifo',
  'lilo',
  'filo',
];

function getValidateFactory(code, validLength) {
  const {
    validateOneOf,
  } = require('internal/validators');

  switch (code) {
    case 'validateOneOf':
      return (n) => validateOneOf(n, 'n', validValues.slice(0, validLength));
  }
}

function main({ n, code, validLength }) {
  const validate = getValidateFactory(code, validLength);

  // Warm up.
  const length = 1024;
  const array = [];

  const value = validValues[validLength - 1];

  for (let i = 0; i < length; ++i) {
    array.push(validate(value));
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    const index = i % length;
    array[index] = validate(value);
  }
  bench.end(n);


  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], 'undefined');
  }
}
