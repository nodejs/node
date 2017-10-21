'use strict';

require('../common');
const vm = require('vm');
const assert = require('assert');

const sym1 = Symbol('1');
const sym2 = Symbol('2');
const sandbox = {
  a: true,
  [sym1]: true
};
Object.defineProperty(sandbox, 'b', { value: true });
Object.defineProperty(sandbox, sym2, { value: true });

const ctx = vm.createContext(sandbox);

// sanity check
assert.deepStrictEqual(Reflect.ownKeys(sandbox), ['a', 'b', sym1, sym2]);
assert.deepStrictEqual(Object.getOwnPropertyNames(sandbox), ['a', 'b']);
assert.deepStrictEqual(Object.getOwnPropertySymbols(sandbox), [sym1, sym2]);

const nativeKeys = vm.runInNewContext('Reflect.ownKeys(this);');
const ownKeys = vm.runInContext('Reflect.ownKeys(this);', ctx);
const restKeys = ownKeys.filter((key) => !nativeKeys.includes(key));
//eslint-disable-next-line no-restricted-properties
assert.deepEqual(restKeys, ['a', 'b', sym1, sym2]);

const nativeNames = vm.runInNewContext('Object.getOwnPropertyNames(this);');
const ownNames = vm.runInContext('Object.getOwnPropertyNames(this);', ctx);
const restNames = ownNames.filter((name) => !nativeNames.includes(name));
//eslint-disable-next-line no-restricted-properties
assert.deepEqual(restNames, ['a', 'b']);

const nativeSym = vm.runInNewContext('Object.getOwnPropertySymbols(this);');
const ownSym = vm.runInContext('Object.getOwnPropertySymbols(this);', ctx);
const restSym = ownSym.filter((sym) => !nativeSym.includes(sym));
//eslint-disable-next-line no-restricted-properties
assert.deepEqual(restSym, [sym1, sym2]);
