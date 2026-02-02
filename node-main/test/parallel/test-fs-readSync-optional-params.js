'use strict';

const { mustNotMutateObjectDeep } = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');

const expected = Buffer.from('xyz\n');

function runTest(defaultBuffer, options, errorCode = false) {
  let fd;
  try {
    fd = fs.openSync(filepath, 'r');
    if (errorCode) {
      assert.throws(
        () => fs.readSync(fd, defaultBuffer, options),
        { code: errorCode }
      );
    } else {
      const result = fs.readSync(fd, defaultBuffer, options);
      assert.strictEqual(result, expected.length);
      assert.deepStrictEqual(defaultBuffer, expected);
    }
  } finally {
    if (fd != null) fs.closeSync(fd);
  }
}

for (const options of [

  // Test options object
  { offset: 0 },
  { length: expected.length },
  { position: 0 },
  { offset: 0, length: expected.length },
  { offset: 0, position: 0 },
  { length: expected.length, position: 0 },
  { offset: 0, length: expected.length, position: 0 },

  { position: null },
  { position: -1 },
  { position: 0n },

  // Test default params
  {},
  null,
  undefined,

  // Test malicious corner case: it works as {length: 4} but not intentionally
  new String('4444'),
]) {
  runTest(Buffer.allocUnsafe(expected.length), options);
}

for (const options of [

  // Test various invalid options
  false,
  true,
  Infinity,
  42n,
  Symbol(),
  'amString',
  [],
  () => {},

  // Test if arbitrary entity with expected .length is not mistaken for options
  '4'.repeat(expected.length),
  [4, 4, 4, 4],
]) {
  runTest(Buffer.allocUnsafe(expected.length), mustNotMutateObjectDeep(options), 'ERR_INVALID_ARG_TYPE');
}
