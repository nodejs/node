// Flags: --experimental-ffi --allow-natives-syntax --no-warnings
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

// Verify strict-numeric signatures still throw ERR_INVALID_ARG_VALUE for
// non-conforming inputs after V8 fast-call engages. Without per-arg
// validation in the wrapper, V8's CheckedNumberAsWord32 / CheckedBigIntTrunc-
// atingWord64 silently truncate out-of-range and non-integer inputs in the
// fast path — yielding tier-dependent semantics where the same FFI binding
// throws cold and silently corrupts hot. The wrapper must run validateAnd-
// Coerce before the fast-call so the contract holds across all tiers.

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

function optimize(fn) {
  // %PrepareFunctionForOptimization marks the function and runs profiling;
  // %OptimizeFunctionOnNextCall forces TurboFan on the next invocation.
  // After that call, the function is fully optimized and the inner
  // v8::Function fast-call path is engaged for matching arg shapes.
  eval('%PrepareFunctionForOptimization(fn)');
  fn();
  eval('%OptimizeFunctionOnNextCall(fn)');
  fn();
}

test('i32 strict validation holds after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  try {
    const callOk = () => assert.strictEqual(functions.add_i32(1, 2), 3);
    optimize(callOk);

    // Out-of-range int: must throw, not silently wrap to -2147483648.
    assert.throws(() => functions.add_i32(2147483648, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    // Non-integer number: must throw, not silently floor to 1.
    assert.throws(() => functions.add_i32(1.5, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    // NaN: must throw, not coerce to 0.
    assert.throws(() => functions.add_i32(NaN, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('u32 strict validation holds after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_u32: { result: 'u32', parameters: ['u32', 'u32'] },
  });
  try {
    const callOk = () => assert.strictEqual(functions.add_u32(1, 2), 3);
    optimize(callOk);

    assert.throws(() => functions.add_u32(-1, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_u32(4294967296, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_u32(1.5, 0),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('i64 strict validation holds after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i64: { result: 'i64', parameters: ['i64', 'i64'] },
  });
  try {
    const callOk = () => assert.strictEqual(functions.add_i64(1n, 2n), 3n);
    optimize(callOk);

    // Out-of-range BigInt: V8's CheckedBigIntTruncatingWord64 would silently
    // wrap to low 64 bits in the fast path. The JS validation must catch it.
    assert.throws(() => functions.add_i64(1n << 100n, 0n),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_i64(1n << 63n, 0n),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    // Number passed where BigInt expected.
    assert.throws(() => functions.add_i64(1, 2),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('u64 strict validation holds after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_u64: { result: 'u64', parameters: ['u64', 'u64'] },
  });
  try {
    const callOk = () => assert.strictEqual(functions.add_u64(1n, 2n), 3n);
    optimize(callOk);

    // Negative BigInt passed to u64: V8 truncation would wrap to 2^64-1.
    assert.throws(() => functions.add_u64(-1n, 0n),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_u64(1n << 64n, 0n),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('f64 type validation holds after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_f64: { result: 'f64', parameters: ['f64', 'f64'] },
  });
  try {
    const callOk = () => assert.strictEqual(functions.add_f64(1.5, 2.5), 4);
    optimize(callOk);

    // Non-number must throw (V8's fast-call accepts any number including
    // NaN/Inf, but rejects non-numbers via deopt to slow path).
    assert.throws(() => functions.add_f64('1', 2),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_f64(1n, 2),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally {
    lib.close();
  }
});

test('arity errors hold after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  try {
    const callOk = () => assert.strictEqual(functions.add_i32(1, 2), 3);
    optimize(callOk);

    assert.throws(() => functions.add_i32(1),
                  { code: 'ERR_INVALID_ARG_VALUE',
                    message: /Invalid argument count: expected 2/ });
    assert.throws(() => functions.add_i32(1, 2, 3),
                  { code: 'ERR_INVALID_ARG_VALUE',
                    message: /Invalid argument count: expected 2/ });
  } finally {
    lib.close();
  }
});

test('close-while-live check holds after fast-call engages', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  const callOk = () => assert.strictEqual(functions.add_i32(1, 2), 3);
  optimize(callOk);

  lib.close();

  assert.throws(() => functions.add_i32(1, 2),
                { code: 'ERR_FFI_LIBRARY_CLOSED' });
});
