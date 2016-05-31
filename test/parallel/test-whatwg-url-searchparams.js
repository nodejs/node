'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const serialized = 'a=a&a=1&a=true&a=undefined&a=null&a=%5Bobject%20Object%5D';
const values = ['a', 1, true, undefined, null, {}];

const m = new URL('http://example.org');
const sp = m.searchParams;

assert(sp);
assert.strictEqual(sp.toString(), '');
assert.strictEqual(m.search, '');

assert(!sp.has('a'));
values.forEach((i) => sp.set('a', i));
assert(sp.has('a'));
assert.strictEqual(sp.get('a'), '[object Object]');
sp.delete('a');
assert(!sp.has('a'));
values.forEach((i) => sp.append('a', i));
assert(sp.has('a'));
assert.strictEqual(sp.getAll('a').length, 6);
assert.strictEqual(sp.get('a'), 'a');

assert.strictEqual(sp.toString(), serialized);

assert.strictEqual(m.search, `?${serialized}`);

var key, val, n = 0;
for ([key, val] of sp) {
  assert.strictEqual(key, 'a');
  assert.strictEqual(val, String(values[n++]));
}
