'use strict';

const common = require('../common');
const assert = require('assert');
const SlowBuffer = require('buffer').SlowBuffer;

const bufferNegativeMsg = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: RangeError,
  message: /^The value "[^"]*" is invalid for option "size"$/
}, 5);
assert.throws(() => Buffer(-1).toString('utf8'), bufferNegativeMsg);
assert.throws(() => SlowBuffer(-1).toString('utf8'), bufferNegativeMsg);
assert.throws(() => Buffer.alloc(-1).toString('utf8'), bufferNegativeMsg);
assert.throws(() => Buffer.allocUnsafe(-1).toString('utf8'), bufferNegativeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(-1).toString('utf8'),
              bufferNegativeMsg);
