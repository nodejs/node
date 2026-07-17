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

test('fast FFI string buffers survive reentrant callbacks', () => {
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
