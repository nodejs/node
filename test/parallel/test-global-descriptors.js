'use strict';

require('../common');
const assert = require('assert');

const actualGlobal = Function('return this')();

const {
  value,
  configurable,
  enumerable,
  writable
} = Object.getOwnPropertyDescriptor(actualGlobal, 'globalThis');

assert.strictEqual(value, actualGlobal, 'globalThis should be global object');
assert.strictEqual(configurable, true, 'globalThis should be configurable');
assert.strictEqual(enumerable, false, 'globalThis should be non-enumerable');
assert.strictEqual(writable, true, 'globalThis should be writable');
