'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { URL, URLSearchParams } = require('url');

// Tests below are not from WPT.
const serialized = 'a=a&a=1&a=true&a=undefined&a=null&a=%EF%BF%BD' +
                   '&a=%EF%BF%BD&a=%F0%9F%98%80&a=%EF%BF%BD%EF%BF%BD' +
                   '&a=%5Bobject+Object%5D';
const values = ['a', 1, true, undefined, null, '\uD83D', '\uDE00',
                '\uD83D\uDE00', '\uDE00\uD83D', {}];
const normalizedValues = ['a', '1', 'true', 'undefined', 'null', '\uFFFD',
                          '\uFFFD', '\uD83D\uDE00', '\uFFFD\uFFFD',
                          '[object Object]'];

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

m.search = '';
assert.strictEqual(sp.toString(), '');

values.forEach((i) => sp.append('a', i));
assert(sp.has('a'));
assert.strictEqual(sp.getAll('a').length, values.length);
assert.strictEqual(sp.get('a'), 'a');

assert.strictEqual(sp.toString(), serialized);

assert.strictEqual(m.search, `?${serialized}`);

assert.strictEqual(sp[Symbol.iterator], sp.entries);

let key, val;
let n = 0;
for ([key, val] of sp) {
  assert.strictEqual(key, 'a', n);
  assert.strictEqual(val, normalizedValues[n], n);
  n++;
}
n = 0;
for (key of sp.keys()) {
  assert.strictEqual(key, 'a', n);
  n++;
}
n = 0;
for (val of sp.values()) {
  assert.strictEqual(val, normalizedValues[n], n);
  n++;
}
n = 0;
sp.forEach(function(val, key, obj) {
  assert.strictEqual(this, undefined, n);
  assert.strictEqual(key, 'a', n);
  assert.strictEqual(val, normalizedValues[n], n);
  assert.strictEqual(obj, sp, n);
  n++;
});
sp.forEach(function() {
  assert.strictEqual(this, m);
}, m);

{
  const callbackErr = common.expectsError({
    code: 'ERR_INVALID_CALLBACK',
    type: TypeError
  }, 2);
  assert.throws(() => sp.forEach(), callbackErr);
  assert.throws(() => sp.forEach(1), callbackErr);
}

m.search = '?a=a&b=b';
assert.strictEqual(sp.toString(), 'a=a&b=b');

const tests = require(path.join(common.fixturesDir, 'url-searchparams.js'));

for (const [input, expected, parsed] of tests) {
  if (input[0] !== '?') {
    const sp = new URLSearchParams(input);
    assert.strictEqual(String(sp), expected);
    assert.deepStrictEqual(Array.from(sp), parsed);

    m.search = input;
    assert.strictEqual(String(m.searchParams), expected);
    assert.deepStrictEqual(Array.from(m.searchParams), parsed);
  }

  {
    const sp = new URLSearchParams(`?${input}`);
    assert.strictEqual(String(sp), expected);
    assert.deepStrictEqual(Array.from(sp), parsed);

    m.search = `?${input}`;
    assert.strictEqual(String(m.searchParams), expected);
    assert.deepStrictEqual(Array.from(m.searchParams), parsed);
  }
}
