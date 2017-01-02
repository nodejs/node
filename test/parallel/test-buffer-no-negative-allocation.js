'use strict';

require('../common');
const assert = require('assert');

const msg = /"size" argument must not be negative/;

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
