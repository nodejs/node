'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const global = vm.runInContext('this', vm.createContext());

const totoSymbol = Symbol.for('toto');
Object.defineProperty(global, totoSymbol, {
  enumerable: true,
  writable: true,
  value: 4,
  configurable: true,
});
assert.strictEqual(global[totoSymbol], 4);
assert.ok(Object.getOwnPropertySymbols(global).includes(totoSymbol));

const totoKey = 'toto';
Object.defineProperty(global, totoKey, {
  enumerable: true,
  writable: true,
  value: 5,
  configurable: true,
});
assert.strictEqual(global[totoKey], 5);
assert.ok(Object.getOwnPropertyNames(global).includes(totoKey));
