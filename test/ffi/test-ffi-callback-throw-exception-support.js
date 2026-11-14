'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const assert = require('node:assert');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');
const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols, { supportsExceptions: true });

let exception;
const callback = lib.registerCallback(
  { arguments: ['i32'], return: 'i32' },
  function callback() {
    exception = new Error('test exception');
    throw exception;
  },
);
function callFFIMethod() {
  return functions.call_int_callback(callback, 21);
}
assert.throws(callFFIMethod, exception);
const stack = exception.stack.split('\n');
assert.match(stack[0], /test exception/);
assert.match(stack[1], /\bcallback\b/);
assert.match(stack[2], /\bcall_int_callback\b/);
assert.match(stack[3], /\bcallFFIMethod\b/);
