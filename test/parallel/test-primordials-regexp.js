// Flags: --expose-internals
'use strict';

const { mustNotCall } = require('../common');
const assert = require('assert');

const {
  RegExpPrototypeSymbolReplace,
  RegExpPrototypeSymbolSearch,
  SafeStringPrototypeSearch,
  hardenRegExp,
} = require('internal/test/binding').primordials;


RegExp.prototype.exec = mustNotCall('%RegExp.prototype%.exec');
RegExp.prototype.test = mustNotCall('%RegExp.prototype%.test');
RegExp.prototype.toString = mustNotCall('%RegExp.prototype%.toString');
RegExp.prototype[Symbol.match] = mustNotCall('%RegExp.prototype%[@@match]');
RegExp.prototype[Symbol.matchAll] = mustNotCall('%RegExp.prototype%[@@matchAll]');
RegExp.prototype[Symbol.replace] = mustNotCall('%RegExp.prototype%[@@replace]');
RegExp.prototype[Symbol.search] = mustNotCall('%RegExp.prototype%[@@search]');
RegExp.prototype[Symbol.split] = mustNotCall('%RegExp.prototype%[@@split]');
Object.defineProperties(RegExp.prototype, {
  dotAll: {
    get: mustNotCall('get %RegExp.prototype%.dotAll'),
  },
  flags: {
    get: mustNotCall('get %RegExp.prototype%.flags'),
  },
  global: {
    get: mustNotCall('get %RegExp.prototype%.global'),
  },
  hasIndices: {
    get: mustNotCall('get %RegExp.prototype%.hasIndices'),
  },
  ignoreCase: {
    get: mustNotCall('get %RegExp.prototype%.ignoreCase'),
  },
  multiline: {
    get: mustNotCall('get %RegExp.prototype%.multiline'),
  },
  source: {
    get: mustNotCall('get %RegExp.prototype%.source'),
  },
  sticky: {
    get: mustNotCall('get %RegExp.prototype%.sticky'),
  },
  unicode: {
    get: mustNotCall('get %RegExp.prototype%.unicode'),
  },
});

hardenRegExp(hardenRegExp(/1/));

// IMO there are no valid use cases in node core to use RegExpPrototypeSymbolMatch
// or RegExpPrototypeSymbolMatchAll, they are inherently unsafe.

{
  const myRegex = hardenRegExp(/a/);
  assert.strictEqual(RegExpPrototypeSymbolReplace(myRegex, 'baar', 'e'), 'bear');
}
{
  const myRegex = hardenRegExp(/a/g);
  assert.strictEqual(RegExpPrototypeSymbolReplace(myRegex, 'baar', 'e'), 'beer');
}
{
  const myRegex = hardenRegExp(/a/);
  assert.strictEqual(RegExpPrototypeSymbolSearch(myRegex, 'baar'), 1);
}
{
  const myRegex = /a/;
  assert.strictEqual(SafeStringPrototypeSearch('baar', myRegex), 1);
}
// RegExpPrototypeSymbolSplit creates a new RegExp instance, and therefore
// does not benefit from `hardenRegExp` and remains unsafe.
// The following test doesn't pass:
// {
//   const myRegex = hardenRegExp(/a/);
//   assert.deepStrictEqual(RegExpPrototypeSymbolSplit(myRegex, 'baar'), ['b', '', 'r']);
// }
