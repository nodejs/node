// Flags: --expose-internals
'use strict';

const { mustNotCall } = require('../common');
const assert = require('assert');

const {
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  RegExpPrototypeSymbolSearch,
  RegExpPrototypeSymbolSplit,
  SafeStringPrototypeSearch,
  hardenRegExp,
} = require('internal/test/binding').primordials;

const {
  SideEffectFreeRegExpPrototypeExec,
  SideEffectFreeRegExpPrototypeSymbolReplace,
  SideEffectFreeRegExpPrototypeSymbolSplit,
} = require('internal/util');


Object.defineProperties(RegExp.prototype, {
  [Symbol.match]: {
    get: mustNotCall('get %RegExp.prototype%[@@match]'),
    set: mustNotCall('set %RegExp.prototype%[@@match]'),
  },
  [Symbol.matchAll]: {
    get: mustNotCall('get %RegExp.prototype%[@@matchAll]'),
    set: mustNotCall('set %RegExp.prototype%[@@matchAll]'),
  },
  [Symbol.replace]: {
    get: mustNotCall('get %RegExp.prototype%[@@replace]'),
    set: mustNotCall('set %RegExp.prototype%[@@replace]'),
  },
  [Symbol.search]: {
    get: mustNotCall('get %RegExp.prototype%[@@search]'),
    set: mustNotCall('set %RegExp.prototype%[@@search]'),
  },
  [Symbol.split]: {
    get: mustNotCall('get %RegExp.prototype%[@@split]'),
    set: mustNotCall('set %RegExp.prototype%[@@split]'),
  },
  dotAll: {
    get: mustNotCall('get %RegExp.prototype%.dotAll'),
    set: mustNotCall('set %RegExp.prototype%.dotAll'),
  },
  exec: {
    get: mustNotCall('get %RegExp.prototype%.exec'),
    set: mustNotCall('set %RegExp.prototype%.exec'),
  },
  flags: {
    get: mustNotCall('get %RegExp.prototype%.flags'),
    set: mustNotCall('set %RegExp.prototype%.flags'),
  },
  global: {
    get: mustNotCall('get %RegExp.prototype%.global'),
    set: mustNotCall('set %RegExp.prototype%.global'),
  },
  hasIndices: {
    get: mustNotCall('get %RegExp.prototype%.hasIndices'),
    set: mustNotCall('set %RegExp.prototype%.hasIndices'),
  },
  ignoreCase: {
    get: mustNotCall('get %RegExp.prototype%.ignoreCase'),
    set: mustNotCall('set %RegExp.prototype%.ignoreCase'),
  },
  multiline: {
    get: mustNotCall('get %RegExp.prototype%.multiline'),
    set: mustNotCall('set %RegExp.prototype%.multiline'),
  },
  source: {
    get: mustNotCall('get %RegExp.prototype%.source'),
    set: mustNotCall('set %RegExp.prototype%.source'),
  },
  sticky: {
    get: mustNotCall('get %RegExp.prototype%.sticky'),
    set: mustNotCall('set %RegExp.prototype%.sticky'),
  },
  test: {
    get: mustNotCall('get %RegExp.prototype%.test'),
    set: mustNotCall('set %RegExp.prototype%.test'),
  },
  toString: {
    get: mustNotCall('get %RegExp.prototype%.toString'),
    set: mustNotCall('set %RegExp.prototype%.toString'),
  },
  unicode: {
    get: mustNotCall('get %RegExp.prototype%.unicode'),
    set: mustNotCall('set %RegExp.prototype%.unicode'),
  },
});

hardenRegExp(hardenRegExp(/1/));

// IMO there are no valid use cases in node core to use RegExpPrototypeSymbolMatch
// or RegExpPrototypeSymbolMatchAll, they are inherently unsafe.

assert.strictEqual(RegExpPrototypeExec(/foo/, 'bar'), null);
assert.strictEqual(RegExpPrototypeExec(hardenRegExp(/foo/), 'bar'), null);
assert.strictEqual(SideEffectFreeRegExpPrototypeExec(/foo/, 'bar'), null);
assert.strictEqual(SideEffectFreeRegExpPrototypeExec(hardenRegExp(/foo/), 'bar'), null);
{
  const expected = ['bar'];
  Object.defineProperties(expected, {
    index: { __proto__: null, configurable: true, writable: true, enumerable: true, value: 0 },
    input: { __proto__: null, configurable: true, writable: true, enumerable: true, value: 'bar' },
    groups: { __proto__: null, configurable: true, writable: true, enumerable: true },
  });
  const actual = SideEffectFreeRegExpPrototypeExec(/bar/, 'bar');

  // assert.deepStrictEqual(actual, expected) doesn't work for cross-realm comparison.

  assert.strictEqual(Array.isArray(actual), Array.isArray(expected));
  assert.deepStrictEqual(Reflect.ownKeys(actual), Reflect.ownKeys(expected));
  for (const key of Reflect.ownKeys(expected)) {
    assert.deepStrictEqual(
      Reflect.getOwnPropertyDescriptor(actual, key),
      Reflect.getOwnPropertyDescriptor(expected, key),
    );
  }
}
{
  const myRegex = hardenRegExp(/a/);
  assert.strictEqual(RegExpPrototypeSymbolReplace(myRegex, 'baar', 'e'), 'bear');
}
{
  const myRegex = /a/;
  assert.strictEqual(SideEffectFreeRegExpPrototypeSymbolReplace(myRegex, 'baar', 'e'), 'bear');
}
{
  const myRegex = hardenRegExp(/a/g);
  assert.strictEqual(RegExpPrototypeSymbolReplace(myRegex, 'baar', 'e'), 'beer');
}
{
  const myRegex = /a/g;
  assert.strictEqual(SideEffectFreeRegExpPrototypeSymbolReplace(myRegex, 'baar', 'e'), 'beer');
}
{
  const myRegex = hardenRegExp(/a/);
  assert.strictEqual(RegExpPrototypeSymbolSearch(myRegex, 'baar'), 1);
}
{
  const myRegex = /a/;
  assert.strictEqual(SafeStringPrototypeSearch('baar', myRegex), 1);
}
{
  const myRegex = hardenRegExp(/a/);
  assert.deepStrictEqual(RegExpPrototypeSymbolSplit(myRegex, 'baar', 0), []);
}
{
  const myRegex = /a/;
  const expected = [];
  const actual = SideEffectFreeRegExpPrototypeSymbolSplit(myRegex, 'baar', 0);

  // assert.deepStrictEqual(actual, expected) doesn't work for cross-realm comparison.

  assert.strictEqual(Array.isArray(actual), Array.isArray(expected));
  assert.deepStrictEqual(Reflect.ownKeys(actual), Reflect.ownKeys(expected));
  for (const key of Reflect.ownKeys(expected)) {
    assert.deepStrictEqual(
      Reflect.getOwnPropertyDescriptor(actual, key),
      Reflect.getOwnPropertyDescriptor(expected, key),
    );
  }
}
{
  const myRegex = hardenRegExp(/a/);
  assert.deepStrictEqual(RegExpPrototypeSymbolSplit(myRegex, 'baar', 1), ['b']);
}
{
  const myRegex = hardenRegExp(/a/);
  assert.deepStrictEqual(RegExpPrototypeSymbolSplit(myRegex, 'baar'), ['b', '', 'r']);
}
