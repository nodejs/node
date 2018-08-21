'use strict';

// Tests below are not from WPT.

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

const params = new URLSearchParams('a=b&c=d');
const values = params.values();

assert.strictEqual(typeof values[Symbol.iterator], 'function');
assert.strictEqual(values[Symbol.iterator](), values);
assert.deepStrictEqual(values.next(), {
  value: 'b',
  done: false
});
assert.deepStrictEqual(values.next(), {
  value: 'd',
  done: false
});
assert.deepStrictEqual(values.next(), {
  value: undefined,
  done: true
});
assert.deepStrictEqual(values.next(), {
  value: undefined,
  done: true
});

common.expectsError(() => {
  values.next.call(undefined);
}, {
  code: 'ERR_INVALID_THIS',
  type: TypeError,
  message: 'Value of "this" must be of type URLSearchParamsIterator'
});
common.expectsError(() => {
  params.values.call(undefined);
}, {
  code: 'ERR_INVALID_THIS',
  type: TypeError,
  message: 'Value of "this" must be of type URLSearchParams'
});
