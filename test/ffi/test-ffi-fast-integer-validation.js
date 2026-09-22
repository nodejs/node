// Flags: --allow-natives-syntax
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

    function callU32(value) { return functions.add_u32(value, 0); }

    function callI64(value) { return functions.add_i64(value, 0); }

    function callU64(value) { return functions.add_u64(value, 0); }

    for (const [fn, value] of [
      [callI8, 0],
      [callU8, 0],
      [callI16, 0],
      [callU16, 0],
      [callI32, 0],
      [callU32, 0],
      [callI64, 0],
      [callU64, 0],
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
    assert.throws(() => callU32(4294967296), expect);
    assert.throws(() => callU32(-1), expect);
    assert.throws(() => callU32(1.5), expect);
    assert.throws(() => callU32('1'), expect);
    assert.strictEqual(callI64(Number.MAX_SAFE_INTEGER),
                       BigInt(Number.MAX_SAFE_INTEGER));
    assert.strictEqual(callI64(Number.MIN_SAFE_INTEGER),
                       BigInt(Number.MIN_SAFE_INTEGER));
    assert.strictEqual(callU64(Number.MAX_SAFE_INTEGER),
                       BigInt(Number.MAX_SAFE_INTEGER));
    assert.strictEqual(callI64((2n ** 63n) - 1n), (2n ** 63n) - 1n);
    assert.strictEqual(callU64((2n ** 64n) - 1n), (2n ** 64n) - 1n);
    assert.throws(() => callI64(Number.MAX_SAFE_INTEGER + 1), expect);
    assert.throws(() => callI64(Number.MIN_SAFE_INTEGER - 1), expect);
    assert.throws(() => callI64(1.5), expect);
    assert.throws(() => callI64(Number.NaN), expect);
    assert.throws(() => callI64(Number.POSITIVE_INFINITY), expect);
    assert.throws(() => callU64(-1), expect);
    assert.throws(() => callU64(Number.MAX_SAFE_INTEGER + 1), expect);
    assert.throws(() => callU64(1.5), expect);
    assert.throws(() => callU64(Number.NaN), expect);
    assert.throws(() => callU64(Number.POSITIVE_INFINITY), expect);
    assert.throws(() => callI64(2n ** 63n), expect);
    assert.throws(() => callU64(2n ** 64n), expect);
  } finally {
    eval('%WaitForBackgroundOptimization()');
    lib.close();
  }
});

test('fast FFI converts single i64/u64 Number arguments before and after optimization', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    identity_i64: { return: 'int64', arguments: ['int64'] },
    identity_u64: { return: 'uint64', arguments: ['uint64'] },
  });

  try {
    // The native signature must have one argument to exercise the single-argument wrapper.
    function callI64(value) { return functions.identity_i64(value); }

    function callU64(value) { return functions.identity_u64(value); }

    for (const optimized of [false, true]) {
      if (optimized) {
        optimize(callI64, -42);
        optimize(callU64, 42);
      }

      for (const value of [0, -0, 42, Number.MAX_SAFE_INTEGER, 0n, 42n]) {
        assert.strictEqual(callI64(value), BigInt(value));
        assert.strictEqual(callU64(value), BigInt(value));
      }
      for (const value of [-42, Number.MIN_SAFE_INTEGER,
                           -(2n ** 63n), (2n ** 63n) - 1n]) {
        assert.strictEqual(callI64(value), BigInt(value));
      }
      assert.strictEqual(callU64((2n ** 64n) - 1n), (2n ** 64n) - 1n);

      const signedError = {
        code: 'ERR_INVALID_ARG_VALUE',
        message: 'Argument 0 must be an int64',
      };
      const unsignedError = {
        code: 'ERR_INVALID_ARG_VALUE',
        message: 'Argument 0 must be a uint64',
      };
      for (const value of [Number.MAX_SAFE_INTEGER + 1, Number.MIN_SAFE_INTEGER - 1,
                           1.5, NaN, Infinity, -Infinity, '1', null, undefined, true, {}]) {
        assert.throws(() => callI64(value), signedError);
        assert.throws(() => callU64(value), unsignedError);
      }
      for (const value of [-(2n ** 63n) - 1n, 2n ** 63n]) {
        assert.throws(() => callI64(value), signedError);
      }
      for (const value of [-1, -1n, 2n ** 64n]) {
        assert.throws(() => callU64(value), unsignedError);
      }
    }
  } finally {
    eval('%WaitForBackgroundOptimization()');
    lib.close();
  }
});

test('fast FFI validates pointer BigInt ranges', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  try {
    for (const type of ['pointer', 'ptr', 'string', 'str',
                        'buffer', 'arraybuffer']) {
      const identityPointer = lib.getFunction('identity_pointer', {
        arguments: [type],
        return: 'pointer',
      });
      const sumBuffer = lib.getFunction('sum_buffer', {
        arguments: [type, 'u64'],
        return: 'u64',
      });
      function callSingle(value) { return identityPointer(value); }

      function callMultiple(value) { return sumBuffer(value, 0n); }

      optimize(callSingle, 0n);
      optimize(callMultiple, 0n);

      const expect = { code: 'ERR_INVALID_ARG_VALUE' };
      for (const call of [callSingle, callMultiple]) {
        assert.throws(() => call(-1n), expect);
        assert.throws(() => call((2n ** 64n) + 5n), expect);
      }
    }
  } finally {
    eval('%WaitForBackgroundOptimization()');
    lib.close();
  }
});
