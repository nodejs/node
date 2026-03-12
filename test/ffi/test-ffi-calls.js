// Flags: --experimental-ffi --expose-gc
'use strict';
const common = require('../common');
const { gcUntil } = require('../common/gc');
const {
  deepStrictEqual,
  ok,
  strictEqual,
  throws,
} = require('node:assert');
const { spawnSync } = require('node:child_process');
const ffi = require('node:ffi');
const { cString, fixtureSymbols, libraryPath } = require('./ffi-test-common');

const { lib, functions: symbols } = ffi.dlopen(libraryPath, fixtureSymbols);

{
  strictEqual(symbols.add_i8(120, 10), -126);
  strictEqual(symbols.add_u8(250, 10), 4);
  strictEqual(symbols.add_i16(10_000, -58), 9_942);
  strictEqual(symbols.add_u16(65_530, 10), 4);
  strictEqual(symbols.add_i32(-10, 52), 42);
  strictEqual(symbols.add_u32(0xFFFFFFFF, 1), 0);
  strictEqual(symbols.add_i64(20n, 22n), 42n);
  strictEqual(symbols.add_u64(20n, 22n), 42n);
}

{
  strictEqual(symbols.add_f32(1.25, 2.75), 4);
  strictEqual(symbols.multiply_f64(6, 7), 42);
  strictEqual(symbols.sum_five_i32(10, 8, 7, 9, 8), 42);
  strictEqual(symbols.sum_five_f64(10, 8, 7, 9, 8), 42);
  strictEqual(symbols.mixed_operation(10, 2.5, 3.5, 4), 20);
}

{
  strictEqual(symbols.logical_and(1, 1), 1);
  strictEqual(symbols.logical_and(1, 0), 0);
  strictEqual(symbols.logical_or(0, 1), 1);
  strictEqual(symbols.logical_not(0), 1);
}

{
  const address = 0x1234n;

  strictEqual(symbols.identity_pointer(address), address);
  strictEqual(symbols.pointer_to_usize(address), address);
  strictEqual(symbols.usize_to_pointer(address), address);
}

{
  strictEqual(symbols.string_length('hello ffi'), 9n);
  strictEqual(symbols.string_length(cString('hello ffi')), 9n);
  strictEqual(symbols.safe_strlen(null), -1);

  const concatenated = symbols.string_concat('hello ', 'ffi');
  strictEqual(typeof concatenated, 'bigint');
  strictEqual(ffi.toString(concatenated), 'hello ffi');
  symbols.free_string(concatenated);

  const duplicated = symbols.string_duplicate('copied string');
  strictEqual(ffi.toString(duplicated), 'copied string');
  symbols.free_string(duplicated);

  const buffer = Buffer.from([1, 2, 3, 4]);
  strictEqual(symbols.sum_buffer(buffer, BigInt(buffer.length)), 10n);
  symbols.reverse_buffer(buffer, BigInt(buffer.length));
  deepStrictEqual([...buffer], [4, 3, 2, 1]);

  const typed = new Uint8Array([5, 6, 7, 8]);
  strictEqual(symbols.sum_buffer(typed, BigInt(typed.byteLength)), 26n);

  const arrayBuffer = new Uint8Array([9, 10, 11, 12]).buffer;
  strictEqual(symbols.sum_buffer(arrayBuffer, BigInt(arrayBuffer.byteLength)), 42n);
}

{
  const ints = new Int32Array([10, 20, 30, 40]);
  strictEqual(symbols.array_get_i32(ints, 2n), 30);
  symbols.array_set_i32(ints, 1n, 22);
  deepStrictEqual([...ints], [10, 22, 30, 40]);

  const doubles = new Float64Array([1, 2, 3, 4]);
  strictEqual(symbols.array_get_f64(doubles, 1n), 2);
  symbols.array_set_f64(doubles, 2n, 39.5);
  deepStrictEqual([...doubles], [1, 2, 39.5, 4]);
}

{
  symbols.reset_counter();
  strictEqual(symbols.get_counter(), 0);
  symbols.increment_counter();
  symbols.increment_counter();
  strictEqual(symbols.get_counter(), 2);
  symbols.reset_counter();
  strictEqual(symbols.get_counter(), 0);
}

{
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
    strictEqual(symbols.call_int_callback(intCallback, 21), 42);
    symbols.call_string_callback(stringCallback, cString('hello callback'));
    deepStrictEqual(seen, ['hello callback']);
    strictEqual(symbols.call_binary_int_callback(binaryCallback, 19, 23), 42);
  } finally {
    lib.unregisterCallback(intCallback);
    lib.unregisterCallback(stringCallback);
    lib.unregisterCallback(binaryCallback);
  }
}

{
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

    strictEqual(called, true);
    deepStrictEqual(values, [0, 1, 2, 3, 4]);
  } finally {
    lib.unregisterCallback(voidCallback);
    lib.unregisterCallback(countingCallback);
  }

  throws(() => lib.refCallback(voidCallback), /Callback not found/);
  throws(() => lib.unregisterCallback(-1n), /The first argument must be a non-negative bigint/);
}

{
  throws(() => symbols.add_i32(1), /Invalid argument count: expected 2, got 1/);
  throws(() => symbols.add_i32('1', 2), /Argument 0 must be an int32/);
  throws(() => symbols.add_i8(1.5, 1), /Argument 0 must be an int8/);
  throws(() => symbols.add_i8(200, 1), /Argument 0 must be an int8/);
  throws(() => symbols.add_u8(Number.NaN, 1), /Argument 0 must be a uint8/);
  throws(() => symbols.add_u8(300, 1), /Argument 0 must be a uint8/);
  throws(() => symbols.add_i16(1.5, 1), /Argument 0 must be an int16/);
  throws(() => symbols.add_i16(40_000, 1), /Argument 0 must be an int16/);
  throws(() => symbols.add_u16(Number.NaN, 1), /Argument 0 must be a uint16/);
  throws(() => symbols.add_u16(70_000, 1), /Argument 0 must be a uint16/);
  throws(() => symbols.add_i64(1, 2n), /Argument 0 must be an int64/);
  throws(() => symbols.add_i64(1.5, 2n), /Argument 0 must be an int64/);
  throws(() => symbols.add_u64('1', 2n), /Argument 0 must be a uint64/);
  throws(() => symbols.add_u64(1, 2n), /Argument 0 must be a uint64/);
  throws(() => symbols.add_u64(Number.NaN, 2n), /Argument 0 must be a uint64/);
  throws(() => symbols.identity_pointer(-1n), /Argument 0 must be a non-negative pointer bigint/);
  throws(() => symbols.string_length(Symbol('x')), /must be a buffer, an ArrayBuffer, a string, or a bigint/);
}

{
  strictEqual(symbols.divide_i32(84, 2), 42);
  strictEqual(symbols.divide_i32(84, 0), 0);
  strictEqual(symbols.safe_strlen(null), -1);
}

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

  ok(common.nodeProcessAborted(status, signal),
     `status: ${status}, signal: ${signal}\nstderr: ${stderr}`);
  ok(/Callback returned invalid value for declared FFI type/.test(stderr));
}

assertInvalidCallbackReturnAborts('1.5');
assertInvalidCallbackReturnAborts('2 ** 40');

(async () => {
  {
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

    strictEqual(symbols.call_int_callback(pointer, 21), 0);

    lib.unregisterCallback(pointer);
  }

  {
    let callback = () => 1;
    const ref = new WeakRef(callback);
    const pointer = lib.registerCallback({ result: 'i32' }, callback);

    lib.unrefCallback(pointer);
    lib.refCallback(pointer);
    callback = null;

    for (let i = 0; i < 5; i++) {
      await gcUntil('ffi refCallback retains callback function', () => true, 1);
      strictEqual(typeof ref.deref(), 'function');
    }

    lib.unregisterCallback(pointer);
  }

  {
    const callback = lib.registerCallback(() => {});

    lib.close();

    throws(() => lib.unregisterCallback(callback), /Library is closed/);
    throws(() => lib.refCallback(callback), /Library is closed/);
    throws(() => lib.unrefCallback(callback), /Library is closed/);
  }
})().then(common.mustCall(), common.mustNotCall());
