// Flags: --expose-internals --allow-natives-syntax
'use strict';

const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const { internalBinding } = require('internal/test/binding');
const {
  kSbSharedBuffer,
} = internalBinding('ffi');

const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('fast FFI accepts buffer and arraybuffer arguments natively', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  const functions = {
    first_byte_buffer: lib.getFunction('first_byte', {
      arguments: ['buffer'],
      return: 'u8',
    }),
    first_byte_arraybuffer: lib.getFunction('first_byte', {
      arguments: ['arraybuffer'],
      return: 'u8',
    }),
    pointer_to_usize: lib.getFunction('pointer_to_usize', {
      arguments: ['pointer'],
      return: 'u64',
    }),
    sum_buffer: {
      arguments: ['buffer', 'u64'],
      return: 'u64',
    },
  };
  functions.sum_buffer = lib.getFunction('sum_buffer', functions.sum_buffer);

  try {
    const bytes = Buffer.from([1, 2, 3, 4]);
    const ab = Uint8Array.from([5, 6, 7, 8]).buffer;

    assert.strictEqual(functions.first_byte_buffer(bytes), 1);
    assert.strictEqual(functions.first_byte_arraybuffer(ab), 5);
    assert.strictEqual(
      functions.pointer_to_usize(bytes), ffi.getRawPointer(bytes));
    assert.strictEqual(functions.sum_buffer(bytes, BigInt(bytes.length)), 10n);

    if (process.arch === 'arm64') {
      assert.strictEqual(functions.first_byte_buffer[kSbSharedBuffer], undefined);
      assert.strictEqual(functions.sum_buffer[kSbSharedBuffer], undefined);
      assert.strictEqual(functions.pointer_to_usize[kSbSharedBuffer], undefined);
    }
  } finally {
    lib.close();
  }
});

test('fast FFI buffer arguments reject invalid values', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    first_byte: { arguments: ['buffer'], return: 'u8' },
  });

  try {
    assert.throws(() => functions.first_byte(123), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  } finally {
    lib.close();
  }
});

test('optimized pointer arguments reject direct SharedArrayBuffers', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  const firstByte = lib.getFunction('first_byte', {
    arguments: ['pointer'],
    return: 'u8',
  });
  const regular = new ArrayBuffer(1);
  const shared = new SharedArrayBuffer(1);
  const expected = { code: 'ERR_INVALID_ARG_VALUE' };

  function callFirstByte(value) {
    return firstByte(value);
  }

  try {
    assert.throws(() => callFirstByte(shared), expected);

    eval('%PrepareFunctionForOptimization(callFirstByte)');
    callFirstByte(regular);
    callFirstByte(regular);
    eval('%OptimizeFunctionOnNextCall(callFirstByte)');
    callFirstByte(regular);

    assert.throws(() => callFirstByte(shared), expected);
  } finally {
    lib.close();
  }
});

test('fast FFI string buffers survive reentrant callbacks', {
  // Bundled libffi callbacks crash on SmartOS.
  skip: common.isSunOS,
}, () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    safe_strlen: { arguments: ['string'], return: 'i32' },
    string_survives_callback: {
      arguments: ['string', 'pointer'],
      return: 'i32',
    },
  });
  let nestedLength;
  const callback = lib.registerCallback(() => {
    nestedLength = functions.safe_strlen('inner string');
  });

  try {
    assert.strictEqual(
      functions.string_survives_callback('outer string', callback), 1);
    assert.strictEqual(nestedLength, 12);
  } finally {
    lib.unregisterCallback(callback);
    lib.close();
  }
});

test('fast FFI refreshes cached temporary string buffers', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  const overwriteString = lib.getFunction('overwrite_string', {
    arguments: ['string', 'i32', 'u64'],
    return: 'u8',
  });

  try {
    const mutated = overwriteString('hello', 0x79, 1n);
    assert.strictEqual(mutated, 0x79);

    const refreshed = overwriteString('hello', 0x79, 0n);
    assert.strictEqual(refreshed, 0x68);
  } finally {
    lib.close();
  }
});

test('optimized buffer signatures preserve pointer-like conversions', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  const asPointer = lib.getFunction('pointer_to_usize', {
    arguments: ['pointer'],
    return: 'u64',
  });
  const asBuffer = lib.getFunction('pointer_to_usize', {
    arguments: ['buffer'],
    return: 'u64',
  });
  const asArrayBuffer = lib.getFunction('pointer_to_usize', {
    arguments: ['arraybuffer'],
    return: 'u64',
  });

  function callPointer(value) {
    return asPointer(value);
  }

  function callBuffer(value) {
    return asBuffer(value);
  }

  function callArrayBuffer(value) {
    return asArrayBuffer(value);
  }

  try {
    for (let i = 0; i < 100_000; i++) {
      assert.strictEqual(callPointer(0n), 0n);
      assert.strictEqual(callBuffer(0n), 0n);
      assert.strictEqual(callArrayBuffer(0n), 0n);
    }

    for (const call of [callPointer, callBuffer, callArrayBuffer]) {
      assert.strictEqual(call(null), 0n);
      assert.strictEqual(call(undefined), 0n);
      assert.notStrictEqual(call('ffi'), 0n);

      const bytes = Buffer.alloc(1);
      assert.strictEqual(call(bytes), ffi.getRawPointer(bytes));

      const arrayBuffer = new ArrayBuffer(8);
      const typedArray = new Uint8Array(arrayBuffer);
      const dataView = new DataView(arrayBuffer);
      arrayBuffer.transfer();

      assert.throws(() => call(arrayBuffer), {
        code: 'ERR_INVALID_ARG_VALUE',
        message: 'Argument 0 is a detached ArrayBuffer',
      });
      for (const view of [typedArray, dataView]) {
        assert.throws(() => call(view), {
          code: 'ERR_INVALID_ARG_VALUE',
          message: 'Argument 0 is an ArrayBufferView backed by a detached ArrayBuffer',
        });
      }
    }
  } finally {
    lib.close();
  }
});

test('multi-argument buffer signatures accept pointer BigInts', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    sum_buffer: { arguments: ['buffer', 'u64'], return: 'u64' },
    fill_buffer: { arguments: ['arraybuffer', 'u64', 'u32'], return: 'void' },
  });

  try {
    const bytes = Buffer.from([1, 2, 3, 4]);
    const pointer = ffi.getRawPointer(bytes);
    const length = BigInt(bytes.length);

    // The two-argument wrapper must treat a raw address like the buffer it
    // came from, matching both the single-argument fast path and the slow
    // paths in src/ffi/types.cc.
    assert.strictEqual(functions.sum_buffer(pointer, length), 10n);
    assert.strictEqual(functions.sum_buffer(bytes, length), 10n);
    assert.strictEqual(functions.sum_buffer(0n, length), 0n);
    assert.strictEqual(functions.sum_buffer(null, length), 0n);

    // The three-argument wrapper must forward the address to real memory
    // instead of rejecting it.
    functions.fill_buffer(pointer, length, 7);
    assert.deepStrictEqual(bytes, Buffer.from([7, 7, 7, 7]));

    // Still accepted once the call has been optimized.
    for (let i = 0; i < 100_000; i++) {
      assert.strictEqual(functions.sum_buffer(pointer, length), 28n);
    }
  } finally {
    lib.close();
  }
});
