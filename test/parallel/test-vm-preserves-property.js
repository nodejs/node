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
assert.strictEqual(typeof res, 'object');
assert.strictEqual(res.value, 'val');
assert.strictEqual(res.configurable, false);
assert.strictEqual(res.enumerable, false);
assert.strictEqual(res.writable, false);
