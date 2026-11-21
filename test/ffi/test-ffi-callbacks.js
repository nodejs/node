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
  assert.throws(() => lib.registerCallback(() => {}, { arguments: ['i32'], return: 'i32' }), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Callback must be the last argument',
  });

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

test('ffi callbacks drain nextTick and microtasks only from the event loop', {
  skip: common.isWindows,
}, async () => {
  const { lib, functions: symbols } = getLibrary();
  let ran = false;
  const syncCallback = lib.registerCallback(
    { arguments: ['i32'], return: 'i32' },
    (value) => {
      queueMicrotask(() => { ran = true; });
      return value;
    },
  );

  // The process exports libuv, so a uv timer can invoke the callback from
  // the event loop with no JS on the stack.
  const { lib: uvLib, functions: uv } = ffi.dlopen(null, {
    uv_handle_size: { arguments: ['i32'], return: 'u64' },
    uv_timer_init: { arguments: ['pointer', 'pointer'], return: 'i32' },
    uv_timer_start: { arguments: ['pointer', 'pointer', 'u64', 'u64'], return: 'i32' },
    uv_close: { arguments: ['pointer', 'pointer'], return: 'void' },
  });
  const UV_TIMER = 13;
  const timerSize = Number(uv.uv_handle_size(UV_TIMER)) + 64;
  const timers = [new Uint8Array(timerSize), new Uint8Array(timerSize)];
  const { promise, resolve } = Promise.withResolvers();
  const order = [];
  const timerCallbacks = [
    uvLib.registerCallback({ arguments: ['pointer'], return: 'void' }, () => {
      order.push('callback');
      process.nextTick(() => order.push('nextTick'));
      queueMicrotask(() => order.push('microtask'));
    }),
    // The second timer runs right after the first one, with no Node.js
    // callback in between that could drain the queues instead.
    uvLib.registerCallback({ arguments: ['pointer'], return: 'void' }, () => {
      resolve([...order]);
      for (const timer of timers) uv.uv_close(timer, 0n);
    }),
  ];

  try {
    // A callback invoked synchronously runs inside the caller's scope, so its
    // microtasks must not run before the native call returns.
    assert.strictEqual(symbols.call_int_callback(syncCallback, 1), 1);
    assert.strictEqual(ran, false);

    const loop = ffi.getCurrentEventLoop();
    for (let i = 0; i < timers.length; i++) {
      assert.strictEqual(uv.uv_timer_init(loop, timers[i]), 0);
      assert.strictEqual(uv.uv_timer_start(timers[i], timerCallbacks[i], 0n, 0n), 0);
    }
    assert.deepStrictEqual(await promise, ['callback', 'nextTick', 'microtask']);
    // Let the uv_close() of the timer handles complete.
    await new Promise(setImmediate);
  } finally {
    lib.close();
    uvLib.close();
  }
});
