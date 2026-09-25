'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { cString, fixtureSymbols, libraryPath } = require('./ffi-test-common');

function getLibrary() {
  return ffi.dlopen(libraryPath, fixtureSymbols);
}

test('ffi callbacks can be registered and invoked', () => {
  const { lib, functions: symbols } = getLibrary();
  const seen = [];
  const intCallback = lib.registerCallback(
    { arguments: ['i32'], return: 'i32' },
    (value) => value * 2,
  );
  const stringCallback = lib.registerCallback(
    { arguments: ['pointer'], return: 'void' },
    (ptr) => seen.push(ffi.toString(ptr)),
  );
  const binaryCallback = lib.registerCallback(
    { arguments: ['i32', 'i32'], return: 'i32' },
    (a, b) => a + b,
  );

  try {
    assert.strictEqual(symbols.call_int_callback(intCallback, 21), 42);
    symbols.call_string_callback(stringCallback, cString('hello callback'));
    assert.deepStrictEqual(seen, ['hello callback']);
    assert.strictEqual(symbols.call_binary_int_callback(binaryCallback, 19, 23), 42);

    const nullPointerCallback = lib.registerCallback({ return: 'pointer' }, () => null);
    const undefinedPointerCallback = lib.registerCallback({ return: 'pointer' }, () => undefined);
    try {
      assert.strictEqual(symbols.call_pointer_callback_is_null(nullPointerCallback), 1);
      assert.strictEqual(symbols.call_pointer_callback_is_null(undefinedPointerCallback), 1);
    } finally {
      lib.unregisterCallback(nullPointerCallback);
      lib.unregisterCallback(undefinedPointerCallback);
    }
  } finally {
    lib.unregisterCallback(intCallback);
    lib.unregisterCallback(stringCallback);
    lib.unregisterCallback(binaryCallback);
    lib.close();
  }
});

test('ffi callback ref and unref APIs work', () => {
  const { lib, functions: symbols } = getLibrary();
  let called = false;
  const values = [];
  const voidCallback = lib.registerCallback(() => {
    called = true;
  });
  const countingCallback = lib.registerCallback(
    { arguments: ['i32'], return: 'i32' },
    (value) => {
      values.push(value);
      return 0;
    },
  );

  try {
    lib.unrefCallback(voidCallback);
    lib.refCallback(voidCallback);
    symbols.call_void_callback(voidCallback);
    symbols.call_callback_multiple_times(countingCallback, 5);

    assert.strictEqual(called, true);
    assert.deepStrictEqual(values, [0, 1, 2, 3, 4]);

    lib.unregisterCallback(voidCallback);
    lib.unregisterCallback(countingCallback);

    assert.throws(() => lib.refCallback(voidCallback), /Callback not found/);
    assert.throws(() => lib.unregisterCallback(-1n), /The first argument must be a non-negative bigint/);
  } finally {
    lib.close();
  }
});
