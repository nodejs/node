'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// Regression test for https://github.com/nodejs/node/issues/22723.

const kFoo = Symbol('kFoo');

const test = {
  'not': 'empty',
  '0': 0,
  '0.5': 0.5,
  '1': 1,
  '-1': -1,
  [kFoo]: kFoo
};
vm.createContext(test);
const proxied = vm.runInContext('this', test);

// TODO(addaleax): The .sort() calls should not be necessary; the property
// order should be indices, then other properties, then symbols.
assert.deepStrictEqual(
  Object.keys(proxied).sort(),
  ['0', '1', 'not', '0.5', '-1'].sort());
assert.deepStrictEqual(
  // Filter out names shared by all global objects, i.e. JS builtins.
  Object.getOwnPropertyNames(proxied)
      .filter((name) => !(name in global))
      .sort(),
  ['0', '1', 'not', '0.5', '-1'].sort());
assert.deepStrictEqual(Object.getOwnPropertySymbols(proxied), []);

Object.setPrototypeOf(test, { inherited: 'true', 17: 42 });

assert.deepStrictEqual(
  Object.keys(proxied).sort(),
  ['0', '1', 'not', '0.5', '-1'].sort());
assert.deepStrictEqual(
  // Filter out names shared by all global objects, i.e. JS builtins.
  Object.getOwnPropertyNames(proxied)
      .filter((name) => !(name in global))
          .sort(),
  ['0', '1', 'not', '0.5', '-1'].sort());
assert.deepStrictEqual(Object.getOwnPropertySymbols(proxied), []);
