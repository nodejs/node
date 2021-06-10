// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const {
  SafeRegExpPrototypeSymbolMatch,
  SafeRegExpPrototypeSymbolReplace,
  SafeRegExpPrototypeSymbolSearch,
  SafeRegExpPrototypeTest,
  SafeRegExpPrototypeSymbolSplit,
  SafeStringPrototypeMatch,
  SafeStringPrototypeMatchAll,
  SafeStringPrototypeSearch,
  SafeStringPrototypeReplace,
  SafeStringPrototypeReplaceAll,
  SafeStringPrototypeSplit,
} = require('internal/test/binding').primordials;

const defineRegExpGetter = (name) =>
  Reflect.defineProperty(RegExp.prototype, name, {
    get: common.mustNotCall(`get RegExp.prototype.${name}`),
  });

RegExp.prototype[Symbol.match] = common.mustNotCall(
  'RegExp.prototype[@@match]'
);
RegExp.prototype[Symbol.matchAll] = common.mustNotCall(
  'RegExp.prototype[@@matchAll]'
);
RegExp.prototype[Symbol.replace] = common.mustNotCall(
  'RegExp.prototype[@@replace]'
);
RegExp.prototype[Symbol.search] = common.mustNotCall(
  'RegExp.prototype[@@search]'
);
RegExp.prototype[Symbol.split] = common.mustNotCall(
  'RegExp.prototype[@@split]'
);
RegExp.prototype.exec = common.mustNotCall('RegExp.prototype.exec');
[
  'flags',
  'global',
  'ignoreCase',
  'multiline',
  'dotAll',
  'unicode',
  'sticky',
].forEach(defineRegExpGetter);

String.prototype[Symbol.match] = common.mustNotCall(
  'String.prototype[@@match]'
);
String.prototype[Symbol.matchAll] = common.mustNotCall(
  'String.prototype[@@matchAll]'
);
String.prototype[Symbol.replace] = common.mustNotCall(
  'String.prototype[@@replace]'
);
String.prototype[Symbol.search] = common.mustNotCall(
  'String.prototype[@@search]'
);
String.prototype[Symbol.split] = common.mustNotCall(
  'String.prototype[@@split]'
);

{
  const expected = ['o'];
  expected.groups = undefined;
  expected.input = 'foo';
  expected.index = 1;
  assert.deepStrictEqual(SafeStringPrototypeMatch('foo', 'o'), expected);
}

assert.deepStrictEqual(SafeStringPrototypeMatchAll('foo', 'o'),
                       ['o', 'o']);

assert.strictEqual(SafeStringPrototypeReplace('foo', 'o', 'a'), 'fao');
assert.strictEqual(SafeStringPrototypeReplaceAll('foo', 'o', 'a'), 'faa');
assert.strictEqual(SafeStringPrototypeSearch('foo', 'o'), 1);
assert.deepStrictEqual(SafeStringPrototypeSplit('foo', 'o'), ['f', '', '']);

{
  const expected = ['o'];
  expected.groups = undefined;
  expected.input = 'foo';
  expected.index = 1;
  assert.deepStrictEqual(SafeRegExpPrototypeSymbolMatch(/o/, 'foo'), expected);
}

assert.strictEqual(SafeRegExpPrototypeSymbolReplace(/o/, 'foo', 'a'), 'fao');
assert.strictEqual(SafeRegExpPrototypeSymbolSearch(/o/, 'foo'), 1);
assert.deepStrictEqual(SafeRegExpPrototypeSymbolSplit(/o/y, 'foo'), ['f', '', '']);
assert.strictEqual(SafeRegExpPrototypeTest(/o/, 'foo'), true);
