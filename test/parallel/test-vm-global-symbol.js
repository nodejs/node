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
