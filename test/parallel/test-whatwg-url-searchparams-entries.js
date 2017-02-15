'use strict';

require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

// Tests below are not from WPT.
const params = new URLSearchParams('a=b&c=d');
const entries = params.entries();
assert.strictEqual(typeof entries[Symbol.iterator], 'function');
assert.strictEqual(entries[Symbol.iterator](), entries);
assert.deepStrictEqual(entries.next(), {
  value: ['a', 'b'],
  done: false
});
assert.deepStrictEqual(entries.next(), {
  value: ['c', 'd'],
  done: false
});
assert.deepStrictEqual(entries.next(), {
  value: undefined,
  done: true
});
assert.deepStrictEqual(entries.next(), {
  value: undefined,
  done: true
});

assert.throws(() => {
  entries.next.call(undefined);
}, /^TypeError: Value of `this` is not a URLSearchParamsIterator$/);
assert.throws(() => {
  params.entries.call(undefined);
}, /^TypeError: Value of `this` is not a URLSearchParams$/);
