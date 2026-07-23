// Flags: --experimental-ffi --allow-natives-syntax
'use strict';

const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

function optimize(fn, value) {
  eval('%PrepareFunctionForOptimization(fn)');
  fn(value);
  fn(value);
  eval('%OptimizeFunctionOnNextCall(fn)');
  fn(value);
}

test('fast FFI validates integer argument ranges', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
  try {
    function callI8(value) { return functions.add_i8(value, 0); }

    function callU8(value) { return functions.add_u8(value, 0); }

    function callI16(value) { return functions.add_i16(value, 0); }

    function callU16(value) { return functions.add_u16(value, 0); }

    function callI32(value) { return functions.add_i32(value, 0); }

    function callI64(value) { return functions.add_i64(value, 0n); }

    function callU64(value) { return functions.add_u64(value, 0n); }

    for (const [fn, value] of [
      [callI8, 0],
      [callU8, 0],
      [callI16, 0],
      [callU16, 0],
      [callI32, 0],
      [callI64, 0n],
      [callU64, 0n],
    ]) {
      optimize(fn, value);
    }

    const expect = { code: 'ERR_INVALID_ARG_VALUE' };
    assert.throws(() => callI8(128), expect);
    assert.throws(() => callU8(256), expect);
    assert.throws(() => callI16(32768), expect);
    assert.throws(() => callU16(65536), expect);
    assert.throws(() => callI32(2147483648), expect);
    assert.throws(() => callI32(-2147483649), expect);
    assert.throws(() => callI32(1.5), expect);
    assert.throws(() => callI32('1'), expect);
    assert.throws(() => callI64(2n ** 63n), expect);
    assert.throws(() => callU64(2n ** 64n), expect);
  } finally {
    lib.close();
  }
});
