'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(typeof globalThis, 'object');
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(globalThis, 'globalThis'),
  { configurable: true, enumerable: false, writable: true, value: globalThis });
assert.strictEqual(globalThis, global);
assert.strictEqual(String(globalThis), '[object global]');
