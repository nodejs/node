'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// These assertions check that we can set new keys to the global context,
// get them back and also list them via getOwnProperty*.
//
// Related to:
// - https://github.com/nodejs/node/issues/45983

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
Object.defineProperty(global, totoSymbol, {
  enumerable: true,
  writable: true,
  value: 44,
  configurable: true,
});
assert.strictEqual(global[totoSymbol], 44);
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
Object.defineProperty(global, totoKey, {
  enumerable: true,
  writable: true,
  value: 55,
  configurable: true,
});
assert.strictEqual(global[totoKey], 55);
assert.ok(Object.getOwnPropertyNames(global).includes(totoKey));

const titiSymbol = Symbol.for('titi');
global[titiSymbol] = 6;
assert.strictEqual(global[titiSymbol], 6);
assert.ok(Object.getOwnPropertySymbols(global).includes(titiSymbol));
global[titiSymbol] = 66;
assert.strictEqual(global[titiSymbol], 66);
assert.ok(Object.getOwnPropertySymbols(global).includes(titiSymbol));

const titiKey = 'titi';
global[titiKey] = 7;
assert.strictEqual(global[titiKey], 7);
assert.ok(Object.getOwnPropertyNames(global).includes(titiKey));
global[titiKey] = 77;
assert.strictEqual(global[titiKey], 77);
assert.ok(Object.getOwnPropertyNames(global).includes(titiKey));

const tutuSymbol = Symbol.for('tutu');
global[tutuSymbol] = () => {};
assert.strictEqual(typeof global[tutuSymbol], 'function');
assert.ok(Object.getOwnPropertySymbols(global).includes(tutuSymbol));
global[tutuSymbol] = () => {};
assert.strictEqual(typeof global[tutuSymbol], 'function');
assert.ok(Object.getOwnPropertySymbols(global).includes(tutuSymbol));

const tutuKey = 'tutu';
global[tutuKey] = () => {};
assert.strictEqual(typeof global[tutuKey], 'function');
assert.ok(Object.getOwnPropertyNames(global).includes(tutuKey));
global[tutuKey] = () => {};
assert.strictEqual(typeof global[tutuKey], 'function');
assert.ok(Object.getOwnPropertyNames(global).includes(tutuKey));
