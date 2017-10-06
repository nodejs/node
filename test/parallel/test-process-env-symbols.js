'use strict';
require('../common');

const assert = require('assert');
const symbol = Symbol('sym');
const errRegExp = /^TypeError: Cannot convert a Symbol value to a string$/;

// Verify that getting via a symbol key returns undefined.
assert.strictEqual(process.env[symbol], undefined);

// Verify that assigning via a symbol key throws.
assert.throws(() => {
  process.env[symbol] = 42;
}, errRegExp);

// Verify that assigning a symbol value throws.
assert.throws(() => {
  process.env.foo = symbol;
}, errRegExp);

// Verify that using a symbol with the in operator returns false.
assert.strictEqual(symbol in process.env, false);

// Verify that deleting a symbol key returns true.
assert.strictEqual(delete process.env[symbol], true);

// Checks that well-known symbols like `Symbol.toStringTag` won’t throw.
assert.doesNotThrow(() => Object.prototype.toString.call(process.env));
