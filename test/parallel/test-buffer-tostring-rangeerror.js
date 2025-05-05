'use strict';
require('../common');

// This test ensures that Node.js throws an Error when trying to convert a
// large buffer into a string.
// Regression test for https://github.com/nodejs/node/issues/649.

const assert = require('assert');
const {
  Buffer,
  constants: {
    MAX_STRING_LENGTH,
  },
} = require('buffer');

const len = MAX_STRING_LENGTH + 1;
const message = {
  code: 'ERR_STRING_TOO_LONG',
  name: 'Error',
};
assert.throws(() => Buffer(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafeSlow(len).toString('utf8'), message);
assert.throws(() => Buffer.alloc(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafe(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafeSlow(len).toString('utf8'), message);
