'use strict';

const common = require('../common');
const assert = require('assert');

const msg = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: RangeError,
  message: /^The value "[^"]*" is invalid for option "size"$/
}, 12);

// Test that negative Buffer length inputs throw errors.

assert.throws(() => Buffer(-Buffer.poolSize), msg);
assert.throws(() => Buffer(-100), msg);
assert.throws(() => Buffer(-1), msg);

assert.throws(() => Buffer.alloc(-Buffer.poolSize), msg);
assert.throws(() => Buffer.alloc(-100), msg);
assert.throws(() => Buffer.alloc(-1), msg);

assert.throws(() => Buffer.allocUnsafe(-Buffer.poolSize), msg);
assert.throws(() => Buffer.allocUnsafe(-100), msg);
assert.throws(() => Buffer.allocUnsafe(-1), msg);

assert.throws(() => Buffer.allocUnsafeSlow(-Buffer.poolSize), msg);
assert.throws(() => Buffer.allocUnsafeSlow(-100), msg);
assert.throws(() => Buffer.allocUnsafeSlow(-1), msg);
