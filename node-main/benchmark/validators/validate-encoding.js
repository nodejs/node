'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e8],
  encoding: [
    'ascii',
    'utf8',
    'utf-8',
    'utf16le',
    'ucs2',
    'ucs-2',
    'base64',
    'latin1',
    'binary',
    'hex',
  ],
  value: [
    'test',
  ],
}, {
  flags: ['--expose-internals'],
});

function getValidateFactory(encoding) {
  const {
    validateEncoding,
  } = require('internal/validators');

  return (n) => validateEncoding(n, encoding);
}

function main({ n, encoding, value }) {
  const validate = getValidateFactory(encoding);

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
