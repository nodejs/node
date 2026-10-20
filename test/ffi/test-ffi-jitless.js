// Flags: --jitless
'use strict';

const common = require('../common');
common.skipIfFFIMissing();
const assert = require('node:assert');
const ffi = require('node:ffi');
const { libraryPath, fixtureSymbols } = require('./ffi-test-common');

const lib = new ffi.DynamicLibrary(libraryPath);
try {
  // A function parameter excludes Fast API, independently of RX support.
  const fallback = lib.getFunction('identity_i64_fallback', {
    arguments: ['pointer', 'i64', 'function'],
    return: 'i64',
  });
  // SAFETY: The fixture ignores both null pointers and returns the scalar value.
  assert.strictEqual(fallback(null, 42n, null), 42n);

  let add;
  try {
    add = lib.getFunction('add_i32', fixtureSymbols.add_i32);
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_RX_MEMORY_NOT_SUPPORTED');
  }
  if (add !== undefined) {
    // SAFETY: The fixture signature matches two int32 arguments and its result.
    assert.strictEqual(add(20, 22), 42);
  }

  let callback;
  try {
    callback = lib.registerCallback({ arguments: ['i32'], return: 'i32' }, (value) => value * 2);
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_RX_MEMORY_NOT_SUPPORTED');
  }
  if (callback !== undefined) {
    try {
      const invoke = lib.getFunction('call_int_callback', {
        arguments: ['function', 'i32'], return: 'i32',
      });
      // SAFETY: The callback is live, runs on this thread, and matches the ABI.
      assert.strictEqual(invoke(callback, 21), 42);
    } finally {
      lib.unregisterCallback(callback);
    }
  }
} finally {
  lib.close();
}
