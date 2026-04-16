// Flags: --experimental-ffi --expose-gc
'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const { gcUntil } = require('../common/gc');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { cString, fixtureSymbols, libraryPath } = require('./ffi-test-common');

function getLibrary() {
  return ffi.dlopen(libraryPath, fixtureSymbols);
}

test('ffi calls support integer arithmetic and char semantics', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    assert.strictEqual(symbols.add_i8(120, 10), -126);
    assert.strictEqual(symbols.add_u8(250, 10), 4);
    assert.strictEqual(symbols.add_i16(10_000, -58), 9_942);
    assert.strictEqual(symbols.add_u16(65_530, 10), 4);
    assert.strictEqual(symbols.add_i32(-10, 52), 42);
    assert.strictEqual(symbols.add_u32(0xFFFFFFFF, 1), 0);
    assert.strictEqual(symbols.add_i64(20n, 22n), 42n);
    assert.strictEqual(symbols.add_u64(20n, 22n), 42n);

    if (symbols.char_is_signed()) {
      assert.strictEqual(symbols.identity_char(-1), -1);
      assert.strictEqual(symbols.identity_char(-128), -128);
    } else {
      assert.strictEqual(symbols.identity_char(255), 255);
      assert.strictEqual(symbols.identity_char(128), 128);
    }
  } finally {
    lib.close();
  }
});

test('ffi calls support floating point and mixed signatures', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    assert.strictEqual(symbols.add_f32(1.25, 2.75), 4);
    assert.strictEqual(symbols.multiply_f64(6, 7), 42);
    assert.strictEqual(symbols.sum_five_i32(10, 8, 7, 9, 8), 42);
    assert.strictEqual(symbols.sum_five_f64(10, 8, 7, 9, 8), 42);
    assert.strictEqual(symbols.mixed_operation(10, 2.5, 3.5, 4), 20);
  } finally {
    lib.close();
  }
});

test('ffi bool signatures use uint8 values', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    assert.strictEqual(symbols.logical_and(1, 1), 1);
    assert.strictEqual(symbols.logical_and(1, 0), 0);
    assert.strictEqual(symbols.logical_or(0, 1), 1);
    assert.strictEqual(symbols.logical_not(0), 1);

    const boolAdder = lib.getFunction('add_u8', {
      parameters: ['bool', 'bool'],
      result: 'bool',
    });
    assert.strictEqual(boolAdder(1, 0), 1);
    assert.throws(() => boolAdder(true, false), /Argument 0 must be a uint8/);
  } finally {
    lib.close();
  }
});

test('ffi pointer identity conversions work', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    const address = 0x1234n;
    assert.strictEqual(symbols.identity_pointer(address), address);
    assert.strictEqual(symbols.pointer_to_usize(address), address);
    assert.strictEqual(symbols.usize_to_pointer(address), address);
  } finally {
    lib.close();
  }
});

test('ffi strings and buffers cross the boundary correctly', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    assert.strictEqual(symbols.string_length('hello ffi'), 9n);
    assert.strictEqual(symbols.string_length(cString('hello ffi')), 9n);
    assert.strictEqual(symbols.safe_strlen(null), -1);

    const concatenated = symbols.string_concat('hello ', 'ffi');
    assert.strictEqual(typeof concatenated, 'bigint');
    assert.strictEqual(ffi.toString(concatenated), 'hello ffi');
    symbols.free_string(concatenated);

    const duplicated = symbols.string_duplicate('copied string');
    assert.strictEqual(ffi.toString(duplicated), 'copied string');
    symbols.free_string(duplicated);

    const buffer = Buffer.from([1, 2, 3, 4]);
    assert.strictEqual(symbols.sum_buffer(buffer, BigInt(buffer.length)), 10n);
    symbols.reverse_buffer(buffer, BigInt(buffer.length));
    assert.deepStrictEqual([...buffer], [4, 3, 2, 1]);

    const typed = new Uint8Array([5, 6, 7, 8]);
    assert.strictEqual(symbols.sum_buffer(typed, BigInt(typed.byteLength)), 26n);

    const arrayBuffer = new Uint8Array([9, 10, 11, 12]).buffer;
    assert.strictEqual(symbols.sum_buffer(arrayBuffer, BigInt(arrayBuffer.byteLength)), 42n);
  } finally {
    lib.close();
  }
});

test('ffi typed array accessors work', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    const ints = new Int32Array([10, 20, 30, 40]);
    assert.strictEqual(symbols.array_get_i32(ints, 2n), 30);
    symbols.array_set_i32(ints, 1n, 22);
    assert.deepStrictEqual([...ints], [10, 22, 30, 40]);

    const doubles = new Float64Array([1, 2, 3, 4]);
    assert.strictEqual(symbols.array_get_f64(doubles, 1n), 2);
    symbols.array_set_f64(doubles, 2n, 39.5);
    assert.deepStrictEqual([...doubles], [1, 2, 39.5, 4]);
  } finally {
    lib.close();
  }
});

test('ffi global state helpers work', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    symbols.reset_counter();
    assert.strictEqual(symbols.get_counter(), 0);
    symbols.increment_counter();
    symbols.increment_counter();
    assert.strictEqual(symbols.get_counter(), 2);
    symbols.reset_counter();
    assert.strictEqual(symbols.get_counter(), 0);
  } finally {
    lib.close();
  }
});

test('ffi callbacks can be registered and invoked', () => {
  const { lib, functions: symbols } = getLibrary();
  const seen = [];
  const intCallback = lib.registerCallback(
    { parameters: ['i32'], result: 'i32' },
    (value) => value * 2,
  );
  const stringCallback = lib.registerCallback(
    { parameters: ['pointer'], result: 'void' },
    (ptr) => seen.push(ffi.toString(ptr)),
  );
  const binaryCallback = lib.registerCallback(
    { arguments: ['i32', 'i32'], returns: 'i32' },
    (a, b) => a + b,
  );

  try {
    assert.strictEqual(symbols.call_int_callback(intCallback, 21), 42);
    symbols.call_string_callback(stringCallback, cString('hello callback'));
    assert.deepStrictEqual(seen, ['hello callback']);
    assert.strictEqual(symbols.call_binary_int_callback(binaryCallback, 19, 23), 42);

    const nullPointerCallback = lib.registerCallback({ result: 'pointer' }, () => null);
    const undefinedPointerCallback = lib.registerCallback({ result: 'pointer' }, () => undefined);
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
    { parameters: ['i32'], result: 'i32' },
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

test('ffi validates invalid arguments', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    assert.throws(() => symbols.add_i32(1), /Invalid argument count: expected 2, got 1/);
    assert.throws(() => symbols.add_i32('1', 2), /Argument 0 must be an int32/);
    assert.throws(() => symbols.add_i8(1.5, 1), /Argument 0 must be an int8/);
    assert.throws(() => symbols.add_i8(200, 1), /Argument 0 must be an int8/);
    assert.throws(() => symbols.add_u8(Number.NaN, 1), /Argument 0 must be a uint8/);
    assert.throws(() => symbols.add_u8(300, 1), /Argument 0 must be a uint8/);
    assert.throws(() => symbols.add_i16(1.5, 1), /Argument 0 must be an int16/);
    assert.throws(() => symbols.add_i16(40_000, 1), /Argument 0 must be an int16/);
    assert.throws(() => symbols.add_u16(Number.NaN, 1), /Argument 0 must be a uint16/);
    assert.throws(() => symbols.add_u16(70_000, 1), /Argument 0 must be a uint16/);
    assert.throws(() => symbols.add_i64(1, 2n), /Argument 0 must be an int64/);
    assert.throws(() => symbols.add_i64(1.5, 2n), /Argument 0 must be an int64/);
    assert.throws(() => symbols.add_i64(2n ** 63n, 2n), /Argument 0 must be an int64/);
    assert.throws(() => symbols.add_i64(-(2n ** 63n) - 1n, 2n), /Argument 0 must be an int64/);
    assert.throws(() => symbols.add_u64('1', 2n), /Argument 0 must be a uint64/);
    assert.throws(() => symbols.add_u64(1, 2n), /Argument 0 must be a uint64/);
    assert.throws(() => symbols.add_u64(Number.NaN, 2n), /Argument 0 must be a uint64/);
    assert.throws(() => symbols.add_u64(-1n, 2n), /Argument 0 must be a uint64/);
    assert.throws(() => symbols.add_u64(2n ** 64n, 2n), /Argument 0 must be a uint64/);
    assert.throws(() => symbols.identity_pointer(-1n), /Argument 0 must be a non-negative pointer bigint/);
    assert.throws(() => symbols.string_length('hello\0ffi'), /Argument 0 must not contain null bytes/);
    assert.throws(() => symbols.string_length(Symbol('x')), /must be a buffer, an ArrayBuffer, a string, or a bigint/);

    if (process.arch === 'ia32' || process.arch === 'arm') {
      assert.throws(() => symbols.identity_pointer(2n ** 32n), /platform pointer range|non-negative pointer bigint/);
    }
  } finally {
    lib.close();
  }
});

test('ffi division helpers behave as expected', () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    assert.strictEqual(symbols.divide_i32(84, 2), 42);
    assert.strictEqual(symbols.divide_i32(84, 0), 0);
    assert.strictEqual(symbols.safe_strlen(null), -1);
  } finally {
    lib.close();
  }
});

function assertInvalidCallbackReturnAborts(returnExpression) {
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '-e',
    `'use strict';
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
const callback = lib.registerCallback(
  { parameters: ['i32'], result: 'i32' },
  () => (${returnExpression}),
);
functions.call_int_callback(callback, 21);`,
  ], {
    encoding: 'utf8',
  });

  assert.ok(common.nodeProcessAborted(status, signal),
            `status: ${status}, signal: ${signal}
stderr: ${stderr}`);
  assert.match(stderr, /Callback returned invalid value for declared FFI type/);
}

function assertInvalidCallbackBehaviorAborts(callbackBody, message) {
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '-e',
    `'use strict';
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
const callback = lib.registerCallback(
  { parameters: ['i32'], result: 'i32' },
  () => { ${callbackBody} },
);
functions.call_int_callback(callback, 21);`,
  ], {
    encoding: 'utf8',
  });

  assert.ok(common.nodeProcessAborted(status, signal),
            `status: ${status}, signal: ${signal}
stderr: ${stderr}`);
  assert.ok(message.test(stderr), stderr);
}

function assertCrossThreadCallbackAbort() {
  const workerSource = `
const { workerData } = require('node:worker_threads');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { functions } = ffi.dlopen(libraryPath, fixtureSymbols);
functions.call_int_callback(workerData, 21);
`;
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '-e',
    `'use strict';
const { Worker } = require('node:worker_threads');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { lib } = ffi.dlopen(libraryPath, fixtureSymbols);
const callback = lib.registerCallback(
  { parameters: ['i32'], result: 'i32' },
  (value) => value * 2,
);
new Worker(${JSON.stringify(workerSource)}, { eval: true, workerData: callback });`,
  ], {
    encoding: 'utf8',
  });

  assert.ok(common.nodeProcessAborted(status, signal),
            `status: ${status}, signal: ${signal}
stderr: ${stderr}`);
  assert.match(stderr, /Callbacks can only be invoked on the system thread they were created on/);
}

test('ffi aborts on invalid callback return values', () => {
  assertInvalidCallbackReturnAborts('1.5');
  assertInvalidCallbackReturnAborts('2 ** 40');
});

test('ffi aborts on invalid callback behavior', () => {
  assertInvalidCallbackBehaviorAborts('throw new Error("boom");', /Callbacks cannot throw an exception/);
  assertInvalidCallbackBehaviorAborts('return Promise.resolve(1);', /Callbacks cannot return promises/);
});

test('ffi aborts on cross-thread callback invocation', () => {
  assertCrossThreadCallbackAbort();
});

test('ffi unrefCallback releases callback function', async () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    let callback = () => 1;
    const ref = new WeakRef(callback);
    const pointer = lib.registerCallback(
      { parameters: ['i32'], result: 'i32' },
      callback,
    );

    lib.unrefCallback(pointer);
    callback = null;

    await gcUntil('ffi unrefCallback releases callback function', () => {
      return ref.deref() === undefined;
    });

    assert.strictEqual(symbols.call_int_callback(pointer, 21), 0);
    lib.unregisterCallback(pointer);
  } finally {
    lib.close();
  }
});

test('ffi unrefCallback zero-fills narrow callback return', async () => {
  const { lib, functions: symbols } = getLibrary();
  try {
    let callback = () => 1;
    const ref = new WeakRef(callback);
    const pointer = lib.registerCallback(
      { parameters: ['i8'], result: 'i8' },
      callback,
    );

    lib.unrefCallback(pointer);
    callback = null;

    await gcUntil('ffi unrefCallback zero-fills narrow callback return', () => {
      return ref.deref() === undefined;
    });

    assert.strictEqual(symbols.call_int8_callback(pointer, 21), 0);
    lib.unregisterCallback(pointer);
  } finally {
    lib.close();
  }
});

test('ffi refCallback retains callback function', async () => {
  const { lib } = getLibrary();
  try {
    let callback = () => 1;
    const ref = new WeakRef(callback);
    const pointer = lib.registerCallback({ result: 'i32' }, callback);

    lib.unrefCallback(pointer);
    lib.refCallback(pointer);
    callback = null;

    for (let i = 0; i < 5; i++) {
      await gcUntil('ffi refCallback retains callback function', () => true, 1);
      assert.strictEqual(typeof ref.deref(), 'function');
    }

    lib.unregisterCallback(pointer);
  } finally {
    lib.close();
  }
});

test('closing a library invalidates callbacks', () => {
  const { lib } = getLibrary();
  const callback = lib.registerCallback(() => {});

  lib.close();

  assert.throws(() => lib.unregisterCallback(callback), /Library is closed/);
  assert.throws(() => lib.refCallback(callback), /Library is closed/);
  assert.throws(() => lib.unrefCallback(callback), /Library is closed/);
});
