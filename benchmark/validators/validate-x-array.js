'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e8],
  type: [
    'validateStringArray',
    'validateBooleanArray',
  ],
  arrayLength: [
    0,
    1,
    10,
    100,
  ],
}, {
  flags: ['--expose-internals'],
});

function getValidateFactory(type, arrayLength) {
  const {
    validateBooleanArray,
    validateStringArray,
  } = require('internal/validators');

  switch (type) {
    case 'validateBooleanArray':
      return [
        (n) => validateBooleanArray(n, 'n'),
        Array.from({ length: arrayLength }, (v, i) => ((i & 1) === 0)),
      ];
    case 'validateStringArray':
      return [
        (n) => validateStringArray(n, 'n'),
        Array.from({ length: arrayLength }, (v, i) => `foo${i}`),
      ];
  }
}

function main({ n, type, arrayLength }) {
  const [validate, value] = getValidateFactory(type, arrayLength);

  // Warm up.
  const length = 1024;
  const array = [];
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
