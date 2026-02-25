// Flags: --experimental-ffi
'use strict';
const common = require('../common');

common.skipIfFFIMissing();

const assert = require('node:assert');
const { strictEqual } = assert;
const { before, describe, it } = require('node:test');
const { dlopen, UnsafePointer, UnsafeCallback } = require('node:ffi');
const path = require('node:path');

const libraryPath = path.join(
  __dirname,
  'fixture_library',
  'build',
  'Release',
  process.platform === 'win32' ? 'ffi_test_library.dll' :
    process.platform === 'darwin' ? 'ffi_test_library.dylib' :
      'ffi_test_library.so',
);

const symbols = {
  // Integer operations
  add_i8: { parameters: ['i8', 'i8'], result: 'i8' },
  add_u8: { parameters: ['u8', 'u8'], result: 'u8' },
  add_i16: { parameters: ['i16', 'i16'], result: 'i16' },
  add_u16: { parameters: ['u16', 'u16'], result: 'u16' },
  add_i32: { parameters: ['i32', 'i32'], result: 'i32' },
  add_u32: { parameters: ['u32', 'u32'], result: 'u32' },
  add_i64: { parameters: ['i64', 'i64'], result: 'i64' },
  add_u64: { parameters: ['u64', 'u64'], result: 'u64' },

  // Floating point operations
  add_f32: { parameters: ['f32', 'f32'], result: 'f32' },
  add_f64: { parameters: ['f64', 'f64'], result: 'f64' },
  multiply_f32: { parameters: ['f32', 'f32'], result: 'f32' },
  multiply_f64: { parameters: ['f64', 'f64'], result: 'f64' },

  // Pointer operations
  identity_pointer: { parameters: ['pointer'], result: 'pointer' },
  pointer_to_usize: { parameters: ['pointer'], result: 'u64' },
  usize_to_pointer: { parameters: ['u64'], result: 'pointer' },

  // String operations
  string_length: { parameters: ['pointer'], result: 'u64' },
  string_duplicate: { parameters: ['pointer'], result: 'pointer' },
  free_string: { parameters: ['pointer'], result: 'void' },

  // Buffer operations (using u32 instead of u8 for the value parameter)
  fill_buffer: { parameters: ['pointer', 'u64', 'u32'], result: 'void' },
  sum_buffer: { parameters: ['pointer', 'u64'], result: 'u64' },
  reverse_buffer: { parameters: ['pointer', 'u64'], result: 'void' },

  // Boolean operations (using i32 instead of u8)
  logical_and: { parameters: ['i32', 'i32'], result: 'i32' },
  logical_or: { parameters: ['i32', 'i32'], result: 'i32' },
  logical_not: { parameters: ['i32'], result: 'i32' },

  // Void operations
  increment_counter: { parameters: [], result: 'void' },
  get_counter: { parameters: [], result: 'i32' },
  reset_counter: { parameters: [], result: 'void' },

  // Callback operations
  call_int_callback: { parameters: ['pointer', 'i32'], result: 'i32' },
  call_void_callback: { parameters: ['pointer'], result: 'void' },
  call_callback_multiple_times: { parameters: ['pointer', 'i32'], result: 'void' },

  // Edge cases
  divide_i32: { parameters: ['i32', 'i32'], result: 'i32' },
  safe_strlen: { parameters: ['pointer'], result: 'i32' },

  // Multi-parameter
  sum_five_i32: { parameters: ['i32', 'i32', 'i32', 'i32', 'i32'], result: 'i32' },
  sum_five_f64: { parameters: ['f64', 'f64', 'f64', 'f64', 'f64'], result: 'f64' },

  // Mixed types
  mixed_operation: { parameters: ['i32', 'f32', 'f64', 'u32'], result: 'f64' },
};

describe('FFI calls into a library', () => {
  let lib;

  before(() => {
    lib = dlopen(libraryPath, symbols);
  });

  describe('Integer operations', () => {
    it('adds two i32 values', () => {
      assert.strictEqual(lib.symbols.add_i32(10, 20), 30);
      assert.strictEqual(lib.symbols.add_i32(-10, 20), 10);
      assert.strictEqual(lib.symbols.add_i32(-10, -20), -30);
    });

    it('adds two u32 values', () => {
      assert.strictEqual(lib.symbols.add_u32(10, 20), 30);
      assert.strictEqual(lib.symbols.add_u32(0xFFFFFFFF, 1), 0);
    });

    it('adds two i64 values', () => {
      assert.strictEqual(lib.symbols.add_i64(100n, 200n), 300n);
      assert.strictEqual(lib.symbols.add_i64(-100n, 200n), 100n);
    });

    it('adds two u64 values', () => {
      assert.strictEqual(lib.symbols.add_u64(100n, 200n), 300n);
    });

    it('adds two i8 values', () => {
      strictEqual(lib.symbols.add_i8(10, 20), 30);
      strictEqual(lib.symbols.add_i8(127, 1), -128); // Overflow
    });

    it('adds two u8 values', () => {
      strictEqual(lib.symbols.add_u8(10, 20), 30);
      strictEqual(lib.symbols.add_u8(255, 1), 0); // Overflow
    });

    it('adds two i16 values', () => {
      strictEqual(lib.symbols.add_i16(100, 200), 300);
      strictEqual(lib.symbols.add_i16(-100, 200), 100);
    });

    it('adds two u16 values', () => {
      strictEqual(lib.symbols.add_u16(100, 200), 300);
    });
  });

  describe('Floating point operations', () => {
    it('adds two f32 values', () => {
      const result = lib.symbols.add_f32(1.5, 2.5);
      assert.ok(Math.abs(result - 4.0) < 0.0001);
    });

    it('adds two f64 values', () => {
      const result = lib.symbols.add_f64(1.5, 2.5);
      assert.strictEqual(result, 4.0);
    });

    it('multiplies two f32 values', () => {
      const result = lib.symbols.multiply_f32(2.0, 3.0);
      assert.ok(Math.abs(result - 6.0) < 0.0001);
    });

    it('multiplies two f64 values', () => {
      const result = lib.symbols.multiply_f64(2.0, 3.0);
      assert.strictEqual(result, 6.0);
    });
  });

  describe('Pointer operations', () => {
    it('returns the same pointer', () => {
      const ptr = UnsafePointer.create(0x1234n);
      const result = lib.symbols.identity_pointer(ptr);
      assert.strictEqual(UnsafePointer.value(result), UnsafePointer.value(ptr));
    });

    it('converts pointer to usize', () => {
      const ptr = UnsafePointer.create(0x1234n);
      const result = lib.symbols.pointer_to_usize(ptr);
      assert.strictEqual(result, 0x1234n);
    });

    it('converts usize to pointer', () => {
      const result = lib.symbols.usize_to_pointer(0x1234n);
      assert.strictEqual(UnsafePointer.value(result), 0x1234n);
    });
  });

  describe('Boolean operations', () => {
    it('performs logical AND', () => {
      assert.strictEqual(lib.symbols.logical_and(1, 1), 1);
      assert.strictEqual(lib.symbols.logical_and(1, 0), 0);
      assert.strictEqual(lib.symbols.logical_and(0, 1), 0);
      assert.strictEqual(lib.symbols.logical_and(0, 0), 0);
    });

    it('performs logical OR', () => {
      assert.strictEqual(lib.symbols.logical_or(1, 1), 1);
      assert.strictEqual(lib.symbols.logical_or(1, 0), 1);
      assert.strictEqual(lib.symbols.logical_or(0, 1), 1);
      assert.strictEqual(lib.symbols.logical_or(0, 0), 0);
    });

    it('performs logical NOT', () => {
      assert.strictEqual(lib.symbols.logical_not(1), 0);
      assert.strictEqual(lib.symbols.logical_not(0), 1);
    });
  });

  describe('Void operations', () => {
    it('increments and reads counter', () => {
      lib.symbols.reset_counter();
      assert.strictEqual(lib.symbols.get_counter(), 0);
      lib.symbols.increment_counter();
      assert.strictEqual(lib.symbols.get_counter(), 1);
      lib.symbols.increment_counter();
      assert.strictEqual(lib.symbols.get_counter(), 2);
      lib.symbols.reset_counter();
      assert.strictEqual(lib.symbols.get_counter(), 0);
    });
  });

  describe('Callback operations', () => {
    it('calls an integer callback', () => {
      const callback = new UnsafeCallback(
        { parameters: ['i32'], result: 'i32' },
        (x) => x * 2,
      );
      const result = lib.symbols.call_int_callback(callback.pointer, 21);
      assert.strictEqual(result, 42);
      callback.close();
    });

    it('calls a void callback', () => {
      let called = false;
      const callback = new UnsafeCallback(
        { parameters: [], result: 'void' },
        () => { called = true; },
      );
      lib.symbols.call_void_callback(callback.pointer);
      assert.strictEqual(called, true);
      callback.close();
    });

    it('calls a callback multiple times', () => {
      const values = [];
      const callback = new UnsafeCallback(
        { parameters: ['i32'], result: 'i32' },
        (x) => { values.push(x); return 0; },
      );
      lib.symbols.call_callback_multiple_times(callback.pointer, 5);
      assert.strictEqual(values.length, 5);
      assert.strictEqual(values[0], 0);
      assert.strictEqual(values[4], 4);
      callback.close();
    });
  });

  describe('Edge cases', () => {
    it('handles division', () => {
      assert.strictEqual(lib.symbols.divide_i32(10, 2), 5);
      assert.strictEqual(lib.symbols.divide_i32(10, 0), 0); // Safe division by zero
    });

    it('handles null pointer in safe_strlen', () => {
      const result = lib.symbols.safe_strlen(UnsafePointer.create(0n));
      assert.strictEqual(result, -1);
    });
  });

  describe('Multi-parameter functions', () => {
    it('sums five i32 values', () => {
      assert.strictEqual(lib.symbols.sum_five_i32(1, 2, 3, 4, 5), 15);
    });

    it('sums five f64 values', () => {
      assert.strictEqual(lib.symbols.sum_five_f64(1.0, 2.0, 3.0, 4.0, 5.0), 15.0);
    });
  });

  describe('Mixed parameter types', () => {
    it('handles mixed types', () => {
      const result = lib.symbols.mixed_operation(10, 2.5, 3.5, 4);
      assert.ok(Math.abs(result - 20.0) < 0.0001);
    });
  });
});
