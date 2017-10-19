'use strict';

require('../common');
const assert = require('assert');

const vm = require('vm');

const x = {};
Object.defineProperty(x, 'prop', {
  configurable: false,
  enumerable: false,
  writable: false,
  value: 'val'
});
const o = vm.createContext(x);

const code = 'Object.getOwnPropertyDescriptor(this, "prop")';
const res = vm.runInContext(code, o, 'test');

assert(res);
assert.strictEqual(typeof res, 'object', `Expected vm.runInContext() to return an object not a(n) ${typeof res}.`);
assert.strictEqual(res.value, 'val', `Expected res.value to equal 'val', got '${res.value}' instead.`);
assert.strictEqual(res.configurable, false, `Expected res.configurable to equal false, got ${res.configurable} instead.`);
assert.strictEqual(res.enumerable, false, `Expected res.enumerable to equal false, got ${res.enumerable} instead.`);
assert.strictEqual(res.writable, false, `Expected res.writable to equal false, got ${res.writable} instead.`);
