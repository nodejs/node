'use strict';
require('../common');
const assert = require('assert');

const qs = require('querystring');

assert.deepStrictEqual(qs.escape(5), '5');
assert.deepStrictEqual(qs.escape('test'), 'test');
assert.deepStrictEqual(qs.escape({}), '%5Bobject%20Object%5D');
assert.deepStrictEqual(qs.escape([5, 10]), '5%2C10');
assert.deepStrictEqual(qs.escape('Ŋōđĕ'), '%C5%8A%C5%8D%C4%91%C4%95');
assert.deepStrictEqual(qs.escape('testŊōđĕ'), 'test%C5%8A%C5%8D%C4%91%C4%95');
assert.deepStrictEqual(qs.escape(`${String.fromCharCode(0xD800 + 1)}test`),
                       '%F0%90%91%B4est');
assert.throws(() => qs.escape(String.fromCharCode(0xD800 + 1)),
              /^URIError: URI malformed$/);

// using toString for objects
assert.strictEqual(
  qs.escape({ test: 5, toString: () => 'test', valueOf: () => 10 }),
  'test'
);

// toString is not callable, must throw an error
assert.throws(() => qs.escape({ toString: 5 }),
              /^TypeError: Cannot convert object to primitive value$/);

// should use valueOf instead of non-callable toString
assert.strictEqual(qs.escape({ toString: 5, valueOf: () => 'test' }), 'test');

assert.throws(() => qs.escape(Symbol('test')),
              /^TypeError: Cannot convert a Symbol value to a string$/);
