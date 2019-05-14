'use strict';
const common = require('../common');

// This test ensures that Node.js throws a RangeError when trying to convert a
// gigantic buffer into a string.
// Regression test for https://github.com/nodejs/node/issues/649.

const assert = require('assert');
const SlowBuffer = require('buffer').SlowBuffer;

const len = 1422561062959;
const message = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: RangeError,
  message: /^The value "[^"]*" is invalid for option "size"$/
}, 5);
assert.throws(() => Buffer(len).toString('utf8'), message);
assert.throws(() => SlowBuffer(len).toString('utf8'), message);
assert.throws(() => Buffer.alloc(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafe(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafeSlow(len).toString('utf8'), message);
