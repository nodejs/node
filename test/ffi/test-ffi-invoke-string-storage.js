// Flags: --expose-internals
'use strict';

const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');
const { DynamicLibrary, kSbInvokeSlow } = internalBinding('ffi');
// Capture the native method before node:ffi installs argument conversions.
const getFunction = DynamicLibrary.prototype.getFunction;
const ffi = require('node:ffi');
const { libraryPath, cString } = require('./ffi-test-common');

const lib = new ffi.DynamicLibrary(libraryPath);
const rawConcat = getFunction.call(lib, 'string_concat', {
  arguments: ['pointer', 'pointer'],
  return: 'pointer',
});
// String and Buffer arguments cannot use the scalar Fast API entrypoint.
// On SharedBuffer platforms, select its native fallback explicitly.
const concat = rawConcat[kSbInvokeSlow] ?? rawConcat;
const free = lib.getFunction('free_string', {
  arguments: ['pointer'],
  return: 'void',
});

try {
  for (const [left, right] of [
    ['', ''],
    ['hello ', 'world'],
    ['a'.repeat(128), 'b'.repeat(128)],
    ['\u03b1', '\u03b2'],
  ]) {
    for (const [a, b] of [
      [left, right],
      [cString(left), right],
      [left, cString(right)],
      [cString(left), cString(right)],
    ]) {
      const result = concat(a, b);
      try {
        assert.strictEqual(ffi.toString(result), left + right);
      } finally {
        free(result);
      }
    }
  }
  assert.throws(() => concat('hello', 'bad\0string'), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => concat('hello', 42), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
} finally {
  lib.close();
}
