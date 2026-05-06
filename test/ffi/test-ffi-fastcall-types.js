// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('i32 add', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.add_i32(20, 22), 42);
    assert.strictEqual(functions.add_i32(-100, 50), -50);
    assert.strictEqual(functions.add_i32(2147483647, 0), 2147483647);
    assert.throws(() => functions.add_i32(2147483648, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('i64 add via BigInt', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i64: { result: 'i64', parameters: ['i64', 'i64'] },
  });
  try {
    assert.strictEqual(functions.add_i64(1n, 2n), 3n);
    assert.strictEqual(functions.add_i64(-1n, 1n), 0n);
    assert.throws(() => functions.add_i64(1, 2),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('i8 add with range checks', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i8: { result: 'i8', parameters: ['i8', 'i8'] },
  });
  try {
    assert.strictEqual(functions.add_i8(10, 20), 30);
    assert.strictEqual(functions.add_i8(-128, 0), -128);
    assert.throws(() => functions.add_i8(128, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_i8(-129, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('u8 add with range checks', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_u8: { result: 'u8', parameters: ['u8', 'u8'] },
  });
  try {
    assert.strictEqual(functions.add_u8(0, 0), 0);
    assert.strictEqual(functions.add_u8(255, 0), 255);
    assert.throws(() => functions.add_u8(-1, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_u8(256, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('float and double round-trip', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_f32: { result: 'f32', parameters: ['f32', 'f32'] },
    add_f64: { result: 'f64', parameters: ['f64', 'f64'] },
  });
  try {
    assert.ok(Math.abs(functions.add_f32(1.5, 2.5) - 4.0) < 1e-6);
    assert.strictEqual(functions.add_f64(1.5, 2.5), 4.0);
  } finally {
    lib.close();
  }
});
