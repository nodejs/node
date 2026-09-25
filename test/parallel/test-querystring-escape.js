'use strict';
require('../common');
const assert = require('assert');

const qs = require('querystring');

assert.strictEqual(qs.escape(5), '5');
assert.strictEqual(qs.escape('test'), 'test');
assert.strictEqual(qs.escape({}), '%5Bobject%20Object%5D');
assert.strictEqual(qs.escape([5, 10]), '5%2C10');
assert.strictEqual(qs.escape('Ŋōđĕ'), '%C5%8A%C5%8D%C4%91%C4%95');
assert.strictEqual(qs.escape('testŊōđĕ'), 'test%C5%8A%C5%8D%C4%91%C4%95');
assert.strictEqual(qs.escape(`${String.fromCharCode(0xD800 + 1)}test`),
                   '%F0%90%91%B4est');

assert.throws(
  () => qs.escape(String.fromCharCode(0xD800 + 1)),
  {
    code: 'ERR_INVALID_URI',
    name: 'URIError',
    message: 'URI malformed'
  }
);

// Using toString for objects
assert.strictEqual(
  qs.escape({ test: 5, toString: () => 'test', valueOf: () => 10 }),
  'test'
);

// `toString` is not callable, must throw an error.
// Error message will vary between different JavaScript engines, so only check
// that it is a `TypeError`.
assert.throws(() => qs.escape({ toString: 5 }), TypeError);

// Should use valueOf instead of non-callable toString.
assert.strictEqual(qs.escape({ toString: 5, valueOf: () => 'test' }), 'test');

// Error message will vary between different JavaScript engines, so only check
// that it is a `TypeError`.
assert.throws(() => qs.escape(Symbol('test')), TypeError);
