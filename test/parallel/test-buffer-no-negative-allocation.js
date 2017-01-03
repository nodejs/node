'use strict';

require('../common');
const assert = require('assert');

const msg = /"size" argument must not be negative/;

// Test that negative Buffer length inputs throw errors.

assert.throws(() => { return Buffer(-Buffer.poolSize); }, msg);
assert.throws(() => { return Buffer(-100); }, msg);
assert.throws(() => { return Buffer(-1); }, msg);

assert.throws(() => { return Buffer.alloc(-Buffer.poolSize); }, msg);
assert.throws(() => { return Buffer.alloc(-100); }, msg);
assert.throws(() => { return Buffer.alloc(-1); }, msg);

assert.throws(() => { return Buffer.allocUnsafe(-Buffer.poolSize); }, msg);
assert.throws(() => { return Buffer.allocUnsafe(-100); }, msg);
assert.throws(() => { return Buffer.allocUnsafe(-1); }, msg);

assert.throws(() => { return Buffer.allocUnsafeSlow(-Buffer.poolSize); }, msg);
assert.throws(() => { return Buffer.allocUnsafeSlow(-100); }, msg);
assert.throws(() => { return Buffer.allocUnsafeSlow(-1); }, msg);
