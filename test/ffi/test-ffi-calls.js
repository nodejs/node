// Flags: --experimental-ffi --expose-gc
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const test = require('node:test');
const { spawnSync } = require('node:child_process');
const ffi = require('node:ffi');
const { cString, fixtureSymbols, libraryPath } = require('./ffi-test-common');

const { lib, functions: symbols } = ffi.dlopen(libraryPath, fixtureSymbols);

test('character operations', (t) => {
  const signed = symbols.char_is_signed();

  t.assert.strictEqual(typeof signed, 'number');
  if (signed) {
    t.assert.strictEqual(symbols.identity_char(-1), -1);
    t.assert.strictEqual(symbols.identity_char(-128), -128);
  } else {
    t.assert.strictEqual(symbols.identity_char(255), 255);
    t.assert.strictEqual(symbols.identity_char(128), 128);
  }
});

test('unsigned integer operations', (t) => {
  t.assert.strictEqual(symbols.add_u8(250, 10), 4);
  t.assert.strictEqual(symbols.add_u16(65_530, 10), 4);
  t.assert.strictEqual(symbols.add_u32(0xFFFFFFFF, 1), 0);
  t.assert.strictEqual(symbols.add_u64(20n, 22n), 42n);
});

test('signed integer operations', (t) => {
  t.assert.strictEqual(symbols.add_i8(120, 10), -126);
  t.assert.strictEqual(symbols.add_i16(10_000, -58), 9_942);
  t.assert.strictEqual(symbols.add_i32(-10, 52), 42);
  t.assert.strictEqual(symbols.add_i64(20n, 22n), 42n);
});

test('floating point and mixed operations', (t) => {
  t.assert.strictEqual(symbols.add_f32(1.25, 2.75), 4);
  t.assert.strictEqual(symbols.multiply_f64(6, 7), 42);
  t.assert.strictEqual(symbols.sum_five_i32(10, 8, 7, 9, 8), 42);
  t.assert.strictEqual(symbols.sum_five_f64(10, 8, 7, 9, 8), 42);
  t.assert.strictEqual(symbols.mixed_operation(10, 2.5, 3.5, 4), 20);
});

test('boolean operations and bool signature validation', (t) => {
  t.assert.strictEqual(symbols.logical_and(1, 1), 1);
  t.assert.strictEqual(symbols.logical_and(1, 0), 0);
  t.assert.strictEqual(symbols.logical_or(0, 1), 1);
  t.assert.strictEqual(symbols.logical_not(0), 1);

  const boolAdder = lib.getFunction('add_u8', {
    parameters: ['bool', 'bool'],
    result: 'bool',
  });
  t.assert.strictEqual(boolAdder(1, 0), 1);
  t.assert.throws(() => boolAdder(true, false), /Argument 0 must be a uint8/);
});

test('pointer roundtrips and conversions', (t) => {
  const address = 0x1234n;

  t.assert.strictEqual(symbols.identity_pointer(address), address);
  t.assert.strictEqual(symbols.pointer_to_usize(address), address);
  t.assert.strictEqual(symbols.usize_to_pointer(address), address);
});

test('string and buffer operations', (t) => {
  t.assert.strictEqual(symbols.string_length('hello ffi'), 9n);
  t.assert.strictEqual(symbols.string_length(cString('hello ffi')), 9n);
  t.assert.strictEqual(symbols.safe_strlen(null), -1);

  const concatenated = symbols.string_concat('hello ', 'ffi');
  t.assert.strictEqual(typeof concatenated, 'bigint');
  t.assert.strictEqual(ffi.toString(concatenated), 'hello ffi');
  symbols.free_string(concatenated);

  const duplicated = symbols.string_duplicate('copied string');
  t.assert.strictEqual(ffi.toString(duplicated), 'copied string');
  symbols.free_string(duplicated);

  const buffer = Buffer.from([1, 2, 3, 4]);
  t.assert.strictEqual(symbols.sum_buffer(buffer, BigInt(buffer.length)), 10n);
  symbols.reverse_buffer(buffer, BigInt(buffer.length));
  t.assert.deepStrictEqual([...buffer], [4, 3, 2, 1]);

  const typed = new Uint8Array([5, 6, 7, 8]);
  t.assert.strictEqual(symbols.sum_buffer(typed, BigInt(typed.byteLength)), 26n);

  const arrayBuffer = new Uint8Array([9, 10, 11, 12]).buffer;
  t.assert.strictEqual(symbols.sum_buffer(arrayBuffer, BigInt(arrayBuffer.byteLength)), 42n);
});

test('typed array get/set operations', (t) => {
  const ints = new Int32Array([10, 20, 30, 40]);
  t.assert.strictEqual(symbols.array_get_i32(ints, 2n), 30);
  symbols.array_set_i32(ints, 1n, 22);
  t.assert.deepStrictEqual([...ints], [10, 22, 30, 40]);

  const doubles = new Float64Array([1, 2, 3, 4]);
  t.assert.strictEqual(symbols.array_get_f64(doubles, 1n), 2);
  symbols.array_set_f64(doubles, 2n, 39.5);
  t.assert.deepStrictEqual([...doubles], [1, 2, 39.5, 4]);
});

test('counter state management', (t) => {
  symbols.reset_counter();
  t.assert.strictEqual(symbols.get_counter(), 0);
  symbols.increment_counter();
  symbols.increment_counter();
  t.assert.strictEqual(symbols.get_counter(), 2);
  symbols.reset_counter();
  t.assert.strictEqual(symbols.get_counter(), 0);
});

test('register and invoke callbacks', (t) => {
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
    t.assert.strictEqual(symbols.call_int_callback(intCallback, 21), 42);
    symbols.call_string_callback(stringCallback, cString('hello callback'));
    t.assert.deepStrictEqual(seen, ['hello callback']);
    t.assert.strictEqual(symbols.call_binary_int_callback(binaryCallback, 19, 23), 42);
  } finally {
    lib.unregisterCallback(intCallback);
    lib.unregisterCallback(stringCallback);
    lib.unregisterCallback(binaryCallback);
  }
});

test('callback ref/unref and callback validation errors', (t) => {
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

    t.assert.strictEqual(called, true);
    t.assert.deepStrictEqual(values, [0, 1, 2, 3, 4]);
  } finally {
    lib.unregisterCallback(voidCallback);
    lib.unregisterCallback(countingCallback);
  }

  t.assert.throws(() => lib.refCallback(voidCallback), /Callback not found/);
  t.assert.throws(() => lib.unregisterCallback(-1n), /The first argument must be a non-negative bigint/);
});

test('argument validation and range errors', (t) => {
  t.assert.throws(() => symbols.add_i32(1), /Invalid argument count: expected 2, got 1/);
  t.assert.throws(() => symbols.add_i32('1', 2), /Argument 0 must be an int32/);
  t.assert.throws(() => symbols.add_i8(1.5, 1), /Argument 0 must be an int8/);
  t.assert.throws(() => symbols.add_i8(200, 1), /Argument 0 must be an int8/);
  t.assert.throws(() => symbols.add_u8(Number.NaN, 1), /Argument 0 must be a uint8/);
  t.assert.throws(() => symbols.add_u8(300, 1), /Argument 0 must be a uint8/);
  t.assert.throws(() => symbols.add_i16(1.5, 1), /Argument 0 must be an int16/);
  t.assert.throws(() => symbols.add_i16(40_000, 1), /Argument 0 must be an int16/);
  t.assert.throws(() => symbols.add_u16(Number.NaN, 1), /Argument 0 must be a uint16/);
  t.assert.throws(() => symbols.add_u16(70_000, 1), /Argument 0 must be a uint16/);
  t.assert.throws(() => symbols.add_i64(1, 2n), /Argument 0 must be an int64/);
  t.assert.throws(() => symbols.add_i64(1.5, 2n), /Argument 0 must be an int64/);
  t.assert.throws(() => symbols.add_i64(2n ** 63n, 2n), /Argument 0 must be an int64/);
  t.assert.throws(() => symbols.add_i64(-(2n ** 63n) - 1n, 2n), /Argument 0 must be an int64/);
  t.assert.throws(() => symbols.add_u64('1', 2n), /Argument 0 must be a uint64/);
  t.assert.throws(() => symbols.add_u64(1, 2n), /Argument 0 must be a uint64/);
  t.assert.throws(() => symbols.add_u64(Number.NaN, 2n), /Argument 0 must be a uint64/);
  t.assert.throws(() => symbols.add_u64(-1n, 2n), /Argument 0 must be a uint64/);
  t.assert.throws(() => symbols.add_u64(2n ** 64n, 2n), /Argument 0 must be a uint64/);
  t.assert.throws(() => symbols.identity_pointer(-1n), /Argument 0 must be a non-negative pointer bigint/);
  t.assert.throws(() => symbols.string_length(Symbol('x')), /must be a buffer, an ArrayBuffer, a string, or a bigint/);

  if (process.arch === 'ia32' || process.arch === 'arm') {
    t.assert.throws(() => symbols.identity_pointer(2n ** 32n), /platform pointer range|non-negative pointer bigint/);
  }
});

test('safe operation behavior', (t) => {
  t.assert.strictEqual(symbols.divide_i32(84, 2), 42);
  t.assert.strictEqual(symbols.divide_i32(84, 0), 0);
  t.assert.strictEqual(symbols.safe_strlen(null), -1);
});

function assertInvalidCallbackReturnAborts(t, returnExpression) {
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

  t.assert.ok(common.nodeProcessAborted(status, signal),
              `status: ${status}, signal: ${signal}\nstderr: ${stderr}`);
  t.assert.match(stderr, /Callback returned invalid value for declared FFI type/);
}

test('invalid callback return aborts for non-integer', (t) => {
  assertInvalidCallbackReturnAborts(t, '1.5');
});

test('invalid callback return aborts for out-of-range integer', (t) => {
  assertInvalidCallbackReturnAborts(t, '2 ** 40');
});
