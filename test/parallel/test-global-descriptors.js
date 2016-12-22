'use strict';
require('../common');

const assert = require('assert');

const {
  value,
  configurable,
  enumerable,
  writable
} = Object.getOwnPropertyDescriptor(global, 'global');

const actualGlobal = Function('return this')();
assert.strictEqual(value, actualGlobal, 'global should be global object');

assert.strictEqual(configurable, true, 'global should be configurable');
assert.strictEqual(enumerable, false, 'global should be non-enumerable');
assert.strictEqual(writable, true, 'global should be writable');
