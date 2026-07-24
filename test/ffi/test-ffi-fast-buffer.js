// Flags: --experimental-ffi --expose-internals
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

test('optimized buffer signatures preserve pointer-like conversions', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  const asBuffer = lib.getFunction('pointer_to_usize', {
    arguments: ['buffer'],
    return: 'u64',
  });
  const asArrayBuffer = lib.getFunction('pointer_to_usize', {
    arguments: ['arraybuffer'],
    return: 'u64',
  });

  function callBuffer(value) {
    return asBuffer(value);
  }

  function callArrayBuffer(value) {
    return asArrayBuffer(value);
  }

  try {
    for (let i = 0; i < 100_000; i++) {
      assert.strictEqual(callBuffer(0n), 0n);
      assert.strictEqual(callArrayBuffer(0n), 0n);
    }

    for (const call of [callBuffer, callArrayBuffer]) {
      assert.strictEqual(call(null), 0n);
      assert.strictEqual(call(undefined), 0n);
      assert.notStrictEqual(call('ffi'), 0n);

      const bytes = Buffer.alloc(1);
      assert.strictEqual(call(bytes), ffi.getRawPointer(bytes));
    }
  } finally {
    lib.close();
  }
});
