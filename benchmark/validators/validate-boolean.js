'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e8],
  code: [
    'validateBoolean',
  ],
  value: [
    'true',
    'false',
  ],
}, {
  flags: ['--expose-internals'],
});

function getValidateFactory(code) {
  const {
    validateBoolean,
  } = require('internal/validators');

  switch (code) {
    case 'validateBoolean':
      return (n) => validateBoolean(n, 'n');
  }
}

function main({ n, code, value }) {
  const validate = getValidateFactory(code);
  const v = value === 'true';

  // Warm up.
  const length = 1024;
  const array = [];
  for (let i = 0; i < length; ++i) {
    array.push(validate(v));
  }

  bench.start();

  for (let i = 0; i < n; ++i) {
    const index = i % length;
    array[index] = validate(v);
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], 'undefined');
  }
}
