'use strict';
require('../common');

const assert = require('assert');
const symbol = Symbol('sym');

// Verify that getting via a symbol key returns undefined.
assert.strictEqual(process.env[symbol], undefined);

// Verify that assigning via a symbol key throws.
// The message depends on the JavaScript engine and so will be different between
// different JavaScript engines. Confirm that the `Error` is a `TypeError` only.
assert.throws(() => {
  process.env[symbol] = 42;
}, TypeError);

// Verify that assigning a symbol value throws.
// The message depends on the JavaScript engine and so will be different between
// different JavaScript engines. Confirm that the `Error` is a `TypeError` only.
assert.throws(() => {
  process.env.foo = symbol;
}, TypeError);

// Verify that using a symbol with the in operator returns false.
assert.strictEqual(symbol in process.env, false);

// Verify that deleting a symbol key returns true.
assert.strictEqual(delete process.env[symbol], true);

// Checks that well-known symbols like `Symbol.toStringTag` won’t throw.
Object.prototype.toString.call(process.env);
