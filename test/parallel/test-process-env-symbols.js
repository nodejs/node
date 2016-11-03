'use strict';
require('../common');

const assert = require('assert');
const symbol = Symbol('sym');
const errRegExp = /^TypeError: Cannot convert a Symbol value to a string$/;

// Verify that getting via a symbol key throws.
assert.throws(() => {
  process.env[symbol];
}, errRegExp);

// Verify that assigning via a symbol key throws.
assert.throws(() => {
  process.env[symbol] = 42;
}, errRegExp);

// Verify that assigning a symbol value throws.
assert.throws(() => {
  process.env.foo = symbol;
}, errRegExp);

// Verify that using a symbol with the in operator throws.
assert.throws(() => {
  symbol in process.env;
}, errRegExp);
