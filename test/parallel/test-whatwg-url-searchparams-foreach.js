'use strict';

const common = require('../common');
const assert = require('assert');
const url = require('url');
const URL = url.URL;
const URLSearchParams = url.URLSearchParams;

let a, b, i;

// ForEach Check
const params = new URLSearchParams('a=1&b=2&c=3');
const keys = [];
const values = [];
params.forEach((value, key) => {
  keys.push(key);
  values.push(value);
});
assert.deepStrictEqual(keys, ['a', 'b', 'c']);
assert.deepStrictEqual(values, ['1', '2', '3']);

// For-of Check
a = new URL('http://a.b/c?a=1&b=2&c=3&d=4');
b = a.searchParams;
const c = [];
for (i of b) {
  a.search = 'x=1&y=2&z=3';
  c.push(i);
}
assert.deepStrictEqual(c[0], ['a', '1']);
assert.deepStrictEqual(c[1], ['y', '2']);
assert.deepStrictEqual(c[2], ['z', '3']);

// empty
a = new URL('http://a.b/c');
b = a.searchParams;
for (i of b) {
  common.fail('should not be reached');
}

assert.throws(() => {
  params.forEach.call(undefined);
}, /^TypeError: Value of `this` is not a URLSearchParams$/);
