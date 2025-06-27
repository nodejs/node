'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

const params = new URLSearchParams('a=b&c=d');
const keys = params.keys();

assert.strictEqual(typeof keys[Symbol.iterator], 'function');
assert.strictEqual(keys[Symbol.iterator](), keys);
assert.deepStrictEqual(keys.next(), {
  value: 'a',
  done: false
});
assert.deepStrictEqual(keys.next(), {
  value: 'c',
  done: false
});
assert.deepStrictEqual(keys.next(), {
  value: undefined,
  done: true
});
assert.deepStrictEqual(keys.next(), {
  value: undefined,
  done: true
});

assert.throws(() => {
  keys.next.call(undefined);
}, {
  code: 'ERR_INVALID_THIS',
  name: 'TypeError',
  message: 'Value of "this" must be of type URLSearchParamsIterator'
});
assert.throws(() => {
  params.keys.call(undefined);
}, {
  code: 'ERR_INVALID_THIS',
  name: 'TypeError',
  message: 'Value of "this" must be of type URLSearchParams'
});
