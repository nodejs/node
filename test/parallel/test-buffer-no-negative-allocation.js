'use strict';

require('../common');
const assert = require('assert');
const { SlowBuffer } = require('buffer');

const msg = {
  code: 'ERR_INVALID_OPT_VALUE',
  name: 'RangeError',
  message: /^The value "[^"]*" is invalid for option "size"$/
};

// Test that negative Buffer length inputs throw errors.

assert.throws(() => Buffer(-Buffer.poolSize), msg);
assert.throws(() => Buffer(-100), msg);
assert.throws(() => Buffer(-1), msg);
assert.throws(() => Buffer(NaN), msg);

assert.throws(() => Buffer.alloc(-Buffer.poolSize), msg);
assert.throws(() => Buffer.alloc(-100), msg);
assert.throws(() => Buffer.alloc(-1), msg);
assert.throws(() => Buffer.alloc(NaN), msg);

assert.throws(() => Buffer.allocUnsafe(-Buffer.poolSize), msg);
assert.throws(() => Buffer.allocUnsafe(-100), msg);
assert.throws(() => Buffer.allocUnsafe(-1), msg);
assert.throws(() => Buffer.allocUnsafe(NaN), msg);

assert.throws(() => Buffer.allocUnsafeSlow(-Buffer.poolSize), msg);
assert.throws(() => Buffer.allocUnsafeSlow(-100), msg);
assert.throws(() => Buffer.allocUnsafeSlow(-1), msg);
assert.throws(() => Buffer.allocUnsafeSlow(NaN), msg);

assert.throws(() => SlowBuffer(-Buffer.poolSize), msg);
assert.throws(() => SlowBuffer(-100), msg);
assert.throws(() => SlowBuffer(-1), msg);
assert.throws(() => SlowBuffer(NaN), msg);
