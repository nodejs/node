// Flags: --experimental-ffi --expose-internals
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');

// Capture the unpatched DynamicLibrary.prototype.getFunction BEFORE loading
// `node:ffi`, which patches it. The SB-metadata test below uses the raw
// method to inspect Symbol-keyed internals that `inheritMetadata`
// deliberately does not forward onto the wrapper.
const { internalBinding } = require('internal/test/binding');
const ffiBinding = internalBinding('ffi');
const {
  kSbInvokeSlow,
  kSbParams,
  kSbResult,
  kSbSharedBuffer,
} = ffiBinding;
const rawGetFunctionUnpatched = ffiBinding.DynamicLibrary.prototype.getFunction;

const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('numeric-only i32 function uses SB path', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.add_i32(20, 22), 42);
    assert.strictEqual(functions.add_i32(-10, 10), 0);
    assert.strictEqual(functions.add_i32(0, 0), 0);
    assert.strictEqual(functions.add_i32(2147483647, 0), 2147483647);
  } finally {
    lib.close();
  }
});

test('i8/u8/i16/u16 round-trip', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i8: { result: 'i8', parameters: ['i8', 'i8'] },
    add_u8: { result: 'u8', parameters: ['u8', 'u8'] },
    add_i16: { result: 'i16', parameters: ['i16', 'i16'] },
    add_u16: { result: 'u16', parameters: ['u16', 'u16'] },
  });
  try {
    assert.strictEqual(functions.add_i8(10, 20), 30);
    assert.strictEqual(functions.add_u8(100, 155), 255);
    assert.strictEqual(functions.add_i16(1000, 2000), 3000);
    assert.strictEqual(functions.add_u16(30000, 35535), 65535);
  } finally {
    lib.close();
  }
});

test('f32/f64 round-trip', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_f32: { result: 'f32', parameters: ['f32', 'f32'] },
    add_f64: { result: 'f64', parameters: ['f64', 'f64'] },
  });
  try {
    // 1.25 and 2.75 are exactly representable in float32, so the sum is exact.
    assert.strictEqual(functions.add_f32(1.25, 2.75), 4.0);
    assert.strictEqual(functions.add_f64(1.5, 2.5), 4.0);
  } finally {
    lib.close();
  }
});

test('i64/u64 BigInt round-trip', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i64: { result: 'i64', parameters: ['i64', 'i64'] },
    add_u64: { result: 'u64', parameters: ['u64', 'u64'] },
  });
  try {
    assert.strictEqual(functions.add_i64(10n, 20n), 30n);
    assert.strictEqual(functions.add_u64(10n, 20n), 30n);
  } finally {
    lib.close();
  }
});

test('zero-arg function', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    char_is_signed: { result: 'i32', parameters: [] },
  });
  try {
    const result = functions.char_is_signed();
    assert.strictEqual(typeof result, 'number');
    assert.ok(result === 0 || result === 1);
  } finally {
    lib.close();
  }
});

test('6-arg numeric function', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    sum_6_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.sum_6_i32(1, 2, 3, 4, 5, 6), 21);
  } finally {
    lib.close();
  }
});

test('pointer args: fast path (BigInt/null) and slow-path fallback (Buffer/ArrayBuffer)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    identity_pointer: { result: 'pointer', parameters: ['pointer'] },
    pointer_to_usize: { result: 'u64', parameters: ['pointer'] },
  });
  try {
    assert.strictEqual(functions.identity_pointer(0n), 0n);
    assert.strictEqual(functions.identity_pointer(0x1234n), 0x1234n);
    assert.strictEqual(functions.identity_pointer(null), 0n);
    assert.strictEqual(functions.identity_pointer(undefined), 0n);
    assert.strictEqual(functions.pointer_to_usize(0x42n), 0x42n);

    const buf = Buffer.from('hello');
    const bufPtr = functions.identity_pointer(buf);
    assert.strictEqual(typeof bufPtr, 'bigint');
    assert.strictEqual(bufPtr, ffi.getRawPointer(buf));

    const abPtr = functions.identity_pointer(new ArrayBuffer(16));
    assert.strictEqual(typeof abPtr, 'bigint');
    assert.ok(abPtr !== 0n);
  } finally {
    lib.close();
  }
});

test('string pointer uses slow-path fallback', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    string_length: { result: 'u64', parameters: ['pointer'] },
  });
  try {
    assert.strictEqual(functions.string_length('hello'), 5n);
    // strlen(NULL) is UB, so use a NUL-terminated Buffer for the fast path.
    assert.strictEqual(functions.string_length(Buffer.from('world\0')), 5n);
  } finally {
    lib.close();
  }
});

test('non-SB-eligible signature falls back to raw function', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    string_duplicate: { result: 'pointer', parameters: ['pointer'] },
    free_string: { result: 'void', parameters: ['pointer'] },
  });
  try {
    const dup = functions.string_duplicate('round-trip');
    assert.strictEqual(typeof dup, 'bigint');
    assert.ok(dup !== 0n);
    functions.free_string(dup);
  } finally {
    lib.close();
  }
});

test('reentrancy across two FFI symbols', () => {
  // A JS callback invoked by one FFI function reenters a different FFI
  // function. Each has its own ArrayBuffer; neither may clobber the other.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    call_int_callback: { result: 'i32', parameters: ['pointer', 'i32'] },
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });

  let callDepth = 0;
  let innerResult = -1;
  const callback = lib.registerCallback(
    { result: 'i32', parameters: ['i32'] },
    (x) => {
      callDepth++;
      if (callDepth === 1) innerResult = functions.add_i32(x, 100);
      return x * 2;
    },
  );

  try {
    const outer = functions.call_int_callback(callback, 7);
    assert.strictEqual(innerResult, 107);
    assert.strictEqual(outer, 14);
  } finally {
    lib.unregisterCallback(callback);
    lib.close();
  }
});

test('arity mismatch throws ERR_INVALID_ARG_VALUE', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  try {
    assert.throws(() => functions.add_i32(1), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /Invalid argument count: expected 2, got 1/,
    });
    assert.throws(() => functions.add_i32(1, 2, 3), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /Invalid argument count: expected 2, got 3/,
    });
  } finally {
    lib.close();
  }
});

test('arity 7+ uses the generic rest-params branch', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    sum_7_i32: {
      result: 'i32',
      parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32'],
    },
  });
  try {
    assert.strictEqual(functions.sum_7_i32(1, 2, 3, 4, 5, 6, 7), 28);
    assert.throws(
      () => functions.sum_7_i32(1, 2, 3, 4, 5, 6),
      { code: 'ERR_INVALID_ARG_VALUE', message: /expected 7, got 6/ },
    );
  } finally {
    lib.close();
  }
});

test('wrappers preserve name/length/pointer and the functions accessor returns wrappers', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
    identity_pointer: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    assert.strictEqual(functions.add_i32.name, 'add_i32');
    assert.strictEqual(typeof functions.add_i32.pointer, 'bigint');
    assert.ok(functions.add_i32.pointer !== 0n);

    assert.strictEqual(functions.identity_pointer.name, 'identity_pointer');
    assert.strictEqual(typeof functions.identity_pointer.pointer, 'bigint');
    assert.ok(functions.identity_pointer.pointer !== 0n);

    // `lib.functions.*` must also go through the SB wrapper.
    assert.strictEqual(typeof lib.functions.add_i32, 'function');
    assert.strictEqual(lib.functions.add_i32(20, 22), 42);
    assert.strictEqual(lib.functions.identity_pointer(0x1234n), 0x1234n);
  } finally {
    lib.close();
  }
});

test('integer boundaries for i8/u8/i16/u16/i32/u32', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i8: { result: 'i8', parameters: ['i8', 'i8'] },
    add_u8: { result: 'u8', parameters: ['u8', 'u8'] },
    add_i16: { result: 'i16', parameters: ['i16', 'i16'] },
    add_u16: { result: 'u16', parameters: ['u16', 'u16'] },
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
    add_u32: { result: 'u32', parameters: ['u32', 'u32'] },
  });

  try {
    assert.strictEqual(functions.add_i8(127, 0), 127);
    assert.strictEqual(functions.add_i8(-128, 0), -128);
    assert.strictEqual(functions.add_u8(255, 0), 255);
    assert.strictEqual(functions.add_u8(0, 0), 0);
    assert.strictEqual(functions.add_i16(32767, 0), 32767);
    assert.strictEqual(functions.add_i16(-32768, 0), -32768);
    assert.strictEqual(functions.add_u16(65535, 0), 65535);
    assert.strictEqual(functions.add_i32(2147483647, 0), 2147483647);
    assert.strictEqual(functions.add_i32(-2147483648, 0), -2147483648);
    assert.strictEqual(functions.add_u32(4294967295, 0), 4294967295);
    assert.strictEqual(functions.add_u32(0, 0), 0);

    const expect = { code: 'ERR_INVALID_ARG_VALUE' };
    assert.throws(() => functions.add_i8(128, 0), expect);
    assert.throws(() => functions.add_i8(-129, 0), expect);
    assert.throws(() => functions.add_u8(256, 0), expect);
    assert.throws(() => functions.add_u8(-1, 0), expect);
    assert.throws(() => functions.add_i16(32768, 0), expect);
    assert.throws(() => functions.add_i16(-32769, 0), expect);
    assert.throws(() => functions.add_u16(65536, 0), expect);
    assert.throws(() => functions.add_u16(-1, 0), expect);
    assert.throws(() => functions.add_i32(2147483648, 0), expect);
    assert.throws(() => functions.add_i32(-2147483649, 0), expect);
    assert.throws(() => functions.add_u32(4294967296, 0), expect);
    assert.throws(() => functions.add_u32(-1, 0), expect);

    assert.throws(() => functions.add_i32(1.5, 0), expect);
    assert.throws(() => functions.add_i32(NaN, 0), expect);
    assert.throws(() => functions.add_i32(Infinity, 0), expect);
    assert.throws(() => functions.add_i32('1', 0), expect);
  } finally {
    lib.close();
  }
});

test('i64/u64 BigInt boundaries and Number/BigInt type mismatches', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i64: { result: 'i64', parameters: ['i64', 'i64'] },
    add_u64: { result: 'u64', parameters: ['u64', 'u64'] },
  });

  try {
    const I64_MAX = (1n << 63n) - 1n;
    const I64_MIN = -(1n << 63n);
    const U64_MAX = (1n << 64n) - 1n;

    assert.strictEqual(functions.add_i64(I64_MAX, 0n), I64_MAX);
    assert.strictEqual(functions.add_i64(I64_MIN, 0n), I64_MIN);
    assert.strictEqual(functions.add_u64(U64_MAX, 0n), U64_MAX);
    assert.strictEqual(functions.add_u64(0n, 0n), 0n);

    const expect = { code: 'ERR_INVALID_ARG_VALUE' };
    assert.throws(() => functions.add_i64(I64_MAX + 1n, 0n), expect);
    assert.throws(() => functions.add_i64(I64_MIN - 1n, 0n), expect);
    assert.throws(() => functions.add_u64(U64_MAX + 1n, 0n), expect);
    assert.throws(() => functions.add_u64(-1n, 0n), expect);

    assert.throws(() => functions.add_i64(1, 2n), expect);
    assert.throws(() => functions.add_i64(1n, '2'), expect);
  } finally {
    lib.close();
  }
});

test('char type picks signed/unsigned range based on host ABI', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    char_is_signed: { result: 'i32', parameters: [] },
    identity_char: { result: 'char', parameters: ['char'] },
  });

  try {
    const isSigned = functions.char_is_signed() !== 0;
    const expect = { code: 'ERR_INVALID_ARG_VALUE' };

    assert.strictEqual(functions.identity_char(65), 65);

    if (isSigned) {
      assert.strictEqual(functions.identity_char(-128), -128);
      assert.strictEqual(functions.identity_char(127), 127);
      assert.throws(() => functions.identity_char(128), expect);
      assert.throws(() => functions.identity_char(-129), expect);
    } else {
      assert.strictEqual(functions.identity_char(255), 255);
      assert.strictEqual(functions.identity_char(0), 0);
      assert.throws(() => functions.identity_char(256), expect);
      assert.throws(() => functions.identity_char(-1), expect);
    }
  } finally {
    lib.close();
  }
});

test('SB metadata is Symbol-keyed, attribute-hardened, and not leaked onto the wrapper', () => {
  const rawLib = new ffiBinding.DynamicLibrary(libraryPath);
  try {
    const rawFn = rawGetFunctionUnpatched.call(
      rawLib, 'add_i32', { result: 'i32', parameters: ['i32', 'i32'] });

    for (const [name, sym] of [
      ['kSbSharedBuffer', kSbSharedBuffer],
      ['kSbInvokeSlow', kSbInvokeSlow],
      ['kSbParams', kSbParams],
      ['kSbResult', kSbResult],
    ]) {
      assert.strictEqual(typeof sym, 'symbol', `${name} must be a Symbol`);
    }

    // Numeric-only signature: kSbInvokeSlow absent; the rest present and hardened.
    for (const [name, sym] of [
      ['kSbSharedBuffer', kSbSharedBuffer],
      ['kSbParams', kSbParams],
      ['kSbResult', kSbResult],
    ]) {
      const desc = Object.getOwnPropertyDescriptor(rawFn, sym);
      assert.ok(desc !== undefined, `${name} missing on pure-numeric SB function`);
      assert.strictEqual(desc.enumerable, false);
      assert.strictEqual(desc.configurable, false);
      assert.strictEqual(desc.writable, false);
    }
    assert.strictEqual(
      Object.getOwnPropertyDescriptor(rawFn, kSbInvokeSlow), undefined);

    // Pointer signature: kSbInvokeSlow must exist (and be hardened).
    const rawPtrFn = rawGetFunctionUnpatched.call(
      rawLib, 'identity_pointer', { result: 'pointer', parameters: ['pointer'] });
    const slowDesc = Object.getOwnPropertyDescriptor(rawPtrFn, kSbInvokeSlow);
    assert.ok(slowDesc !== undefined);
    assert.strictEqual(slowDesc.enumerable, false);
    assert.strictEqual(slowDesc.configurable, false);
    assert.strictEqual(slowDesc.writable, false);

    assert.deepStrictEqual(Object.keys(rawFn), ['pointer']);
    const ownSyms = Object.getOwnPropertySymbols(rawFn);
    assert.ok(ownSyms.includes(kSbSharedBuffer));
    assert.ok(ownSyms.includes(kSbParams));
    assert.ok(ownSyms.includes(kSbResult));

    // Internals must not be forwarded by `inheritMetadata`.
    const { lib, functions } = ffi.dlopen(libraryPath, {
      add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
    });
    try {
      assert.strictEqual(functions.add_i32[kSbSharedBuffer], undefined);
      assert.strictEqual(functions.add_i32[kSbInvokeSlow], undefined);
      assert.strictEqual(functions.add_i32[kSbParams], undefined);
      assert.strictEqual(functions.add_i32[kSbResult], undefined);
    } finally {
      lib.close();
    }
  } finally {
    rawLib.close();
  }
});

test('pointer fast-path range check: [0, 2^64 - 1]', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    identity_pointer: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    assert.strictEqual(functions.identity_pointer(0n), 0n);
    assert.strictEqual(functions.identity_pointer((1n << 64n) - 1n), (1n << 64n) - 1n);

    const expect = { code: 'ERR_INVALID_ARG_VALUE' };
    assert.throws(() => functions.identity_pointer(-1n), expect);
    assert.throws(() => functions.identity_pointer(1n << 64n), expect);
  } finally {
    lib.close();
  }
});

test('self-recursive reentrancy: a single function\'s ArrayBuffer survives a nested call', () => {
  // Stricter invariant than the two-symbol case: `InvokeFunctionSB` must
  // copy args out of the ArrayBuffer to stack before `ffi_call` so a recursive
  // call can reuse the same buffer without clobbering the outer frame.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    call_binary_int_callback: {
      result: 'i32',
      parameters: ['function', 'i32', 'i32'],
    },
  });

  try {
    let depth = 0;
    const callback = lib.registerCallback(
      { result: 'i32', parameters: ['i32', 'i32'] },
      common.mustCall((a, b) => {
        depth++;
        if (depth === 1) {
          const inner = functions.call_binary_int_callback(callback, 100, 200);
          assert.strictEqual(inner, 300);
        }
        return a + b;
      }, 2),
    );

    try {
      assert.strictEqual(functions.call_binary_int_callback(callback, 10, 20), 30);
    } finally {
      lib.unregisterCallback(callback);
    }
  } finally {
    lib.close();
  }
});

test('void-return 0-arg wrapper branch', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    reset_counter: { result: 'void', parameters: [] },
    increment_counter: { result: 'void', parameters: [] },
    get_counter: { result: 'i32', parameters: [] },
  });
  try {
    assert.strictEqual(functions.reset_counter(), undefined);
    assert.strictEqual(functions.get_counter(), 0);

    functions.increment_counter();
    functions.increment_counter();
    functions.increment_counter();
    assert.strictEqual(functions.get_counter(), 3);

    assert.strictEqual(functions.reset_counter(), undefined);
    assert.strictEqual(functions.get_counter(), 0);
  } finally {
    lib.close();
  }
});

test('void-return wrapper at every specialized arity observes side effects', () => {
  // The arity ladder has a separate void-return closure for each arity.
  // A wiring bug in a mid-arity void specialization would not be caught
  // by the 0-arg void test above, so exercise the side effects directly
  // at every arity the ladder specializes (1..6) plus the 7+ rest-params
  // fallback.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    store_i32: { result: 'void', parameters: ['i32'] },
    store_sum_2_i32: { result: 'void', parameters: ['i32', 'i32'] },
    store_sum_3_i32: { result: 'void', parameters: ['i32', 'i32', 'i32'] },
    store_sum_4_i32: {
      result: 'void',
      parameters: ['i32', 'i32', 'i32', 'i32'],
    },
    store_sum_5_i32: {
      result: 'void',
      parameters: ['i32', 'i32', 'i32', 'i32', 'i32'],
    },
    store_sum_6_i32: {
      result: 'void',
      parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32'],
    },
    store_sum_8_i32: {
      result: 'void',
      parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32'],
    },
    get_scratch: { result: 'i32', parameters: [] },
  });
  try {
    // Powers-of-two summands detect a dropped or duplicated slot at each
    // arity.
    assert.strictEqual(functions.store_i32(7), undefined);
    assert.strictEqual(functions.get_scratch(), 7);

    assert.strictEqual(functions.store_sum_2_i32(10, 32), undefined);
    assert.strictEqual(functions.get_scratch(), 42);

    assert.strictEqual(functions.store_sum_3_i32(1, 2, 4), undefined);
    assert.strictEqual(functions.get_scratch(), 7);

    assert.strictEqual(functions.store_sum_4_i32(1, 2, 4, 8), undefined);
    assert.strictEqual(functions.get_scratch(), 15);

    assert.strictEqual(functions.store_sum_5_i32(1, 2, 4, 8, 16), undefined);
    assert.strictEqual(functions.get_scratch(), 31);

    assert.strictEqual(
      functions.store_sum_6_i32(1, 2, 4, 8, 16, 32), undefined);
    assert.strictEqual(functions.get_scratch(), 63);

    // 7+ args takes the generic rest-params void branch rather than a
    // per-arity specialization.
    assert.strictEqual(
      functions.store_sum_8_i32(1, 2, 4, 8, 16, 32, 64, 128), undefined);
    assert.strictEqual(functions.get_scratch(), 255);

    // Validation still runs on every void-return branch, including the
    // rest-params fallback.
    assert.throws(
      () => functions.store_i32(1.5),
      { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(
      () => functions.store_sum_2_i32(1.5, 2),
      { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(
      () => functions.store_sum_3_i32(1, 1.5, 3),
      { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(
      () => functions.store_sum_4_i32(1, 2, 1.5, 4),
      { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(
      () => functions.store_sum_5_i32(1, 2, 3, 1.5, 5),
      { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(
      () => functions.store_sum_6_i32(1, 2, 3, 4, 5),
      { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(
      () => functions.store_sum_8_i32(1, 2, 3, 4, 5, 6, 7, 1.5),
      { code: 'ERR_INVALID_ARG_VALUE' });

    // Wrong arity hits the `throwFFIArgCountError` branch inside each
    // specialization (1..6 and the 7+ rest-params fallback).
    for (const [name, expected, badArgs] of [
      ['store_i32', 1, []],
      ['store_sum_2_i32', 2, [1]],
      ['store_sum_3_i32', 3, [1, 2]],
      ['store_sum_4_i32', 4, [1, 2, 3]],
      ['store_sum_5_i32', 5, [1, 2, 3, 4]],
      ['store_sum_6_i32', 6, [1, 2, 3, 4, 5]],
      ['store_sum_8_i32', 8, [1, 2, 3, 4, 5, 6, 7]],
    ]) {
      assert.throws(
        () => functions[name](...badArgs),
        {
          code: 'ERR_INVALID_ARG_VALUE',
          message: new RegExp(`expected ${expected}, got ${badArgs.length}`),
        });
    }
  } finally {
    lib.close();
  }
});

test('value-return wrapper arity mismatch hits every specialized branch', () => {
  // `sum_7_i32` already exercises the 7+ rest-params branch elsewhere;
  // this test targets the per-arity `throwFFIArgCountError` call in the
  // value-return closures for arities 1..6 so each specialization's
  // argument-count guard runs at least once.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    logical_not: { result: 'i32', parameters: ['i32'] },
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
    sum_3_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32'] },
    sum_4_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32'] },
    sum_five_i32: {
      result: 'i32',
      parameters: ['i32', 'i32', 'i32', 'i32', 'i32'],
    },
    sum_6_i32: {
      result: 'i32',
      parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32'],
    },
  });
  try {
    for (const [name, expected, badArgs] of [
      ['logical_not', 1, []],
      ['add_i32', 2, [1]],
      ['sum_3_i32', 3, [1, 2]],
      ['sum_4_i32', 4, [1, 2, 3]],
      ['sum_five_i32', 5, [1, 2, 3, 4]],
      ['sum_6_i32', 6, [1, 2, 3, 4, 5]],
    ]) {
      assert.throws(
        () => functions[name](...badArgs),
        {
          code: 'ERR_INVALID_ARG_VALUE',
          message: new RegExp(`expected ${expected}, got ${badArgs.length}`),
        });
    }

    // Sanity-check that a correct call still returns a value at each
    // arity — a bug that swallowed the return on the value-return path
    // would be caught here.
    assert.strictEqual(functions.logical_not(0), 1);
    assert.strictEqual(functions.add_i32(1, 2), 3);
    assert.strictEqual(functions.sum_3_i32(1, 2, 4), 7);
    assert.strictEqual(functions.sum_4_i32(1, 2, 4, 8), 15);
    assert.strictEqual(functions.sum_five_i32(1, 2, 4, 8, 16), 31);
    assert.strictEqual(functions.sum_6_i32(1, 2, 4, 8, 16, 32), 63);
  } finally {
    lib.close();
  }
});

test('pointer-dispatch wrapper rejects wrong-arity calls', () => {
  // Pointer signatures share a single rest-params wrapper rather than the
  // per-arity ladder, but it still has its own `throwFFIArgCountError`
  // branch that needs to be exercised.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    identity_pointer: { result: 'pointer', parameters: ['pointer'] },
  });
  try {
    assert.throws(
      () => functions.identity_pointer(),
      {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /expected 1, got 0/,
      });
    assert.throws(
      () => functions.identity_pointer(0n, 0n),
      {
        code: 'ERR_INVALID_ARG_VALUE',
        message: /expected 1, got 2/,
      });
  } finally {
    lib.close();
  }
});

test('mid-arity wrappers (1, 3, 4, 5)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    logical_not: { result: 'i32', parameters: ['i32'] },
    sum_3_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32'] },
    sum_4_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32'] },
    sum_five_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.logical_not(0), 1);
    assert.strictEqual(functions.logical_not(42), 0);

    // Powers-of-two summands: a dropped or duplicated slot would change the total.
    assert.strictEqual(functions.sum_3_i32(1, 2, 4), 7);
    assert.strictEqual(functions.sum_4_i32(1, 2, 4, 8), 15);
    assert.strictEqual(functions.sum_five_i32(1, 2, 4, 8, 16), 31);
  } finally {
    lib.close();
  }
});

test('float specials: NaN, ±Infinity, -0 round-trip bit-exact', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_f64: { result: 'f64', parameters: ['f64', 'f64'] },
    multiply_f64: { result: 'f64', parameters: ['f64', 'f64'] },
  });
  try {
    assert.ok(Number.isNaN(functions.add_f64(NaN, 1.0)));
    assert.strictEqual(functions.add_f64(Infinity, 1.0), Infinity);
    assert.strictEqual(functions.add_f64(-Infinity, 1.0), -Infinity);
    assert.ok(Object.is(functions.multiply_f64(-0, 1.0), -0));
  } finally {
    lib.close();
  }
});

test('arity-7+ branch still runs per-arg validation', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    sum_7_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.throws(
      () => functions.sum_7_i32(1, 2, 3, 1.5, 5, 6, 7),
      { code: 'ERR_INVALID_ARG_VALUE' },
    );
  } finally {
    lib.close();
  }
});

test('mixed-kind signature (i32, f32, f64, u32) dispatches the right writer per slot', () => {
  // Four distinct `sbTypeInfo.kind` values (int, float, float, int) — a
  // wiring bug that reused one writer across slots would surface here.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    mixed_operation: { parameters: ['i32', 'f32', 'f64', 'u32'], result: 'f64' },
  });

  try {
    assert.strictEqual(functions.mixed_operation(10, 2.5, 3.5, 4), 20);
    assert.strictEqual(functions.mixed_operation(-1, 0.25, 0.75, 0), 0);

    const expect = { code: 'ERR_INVALID_ARG_VALUE' };
    // -1 on u32 slot: distinguishes u32 writer from i32 (i32 accepts -1).
    assert.throws(() => functions.mixed_operation(0, 0.0, 0.0, -1), expect);
    // 2^31 on i32 slot: distinguishes i32 writer from u32 (u32 accepts it).
    assert.throws(() => functions.mixed_operation(2147483648, 0.0, 0.0, 0), expect);
    // Float slots reject BigInt / string (the int/float writers both gate on `typeof`).
    assert.throws(() => functions.mixed_operation(0, 1n, 0.0, 0), expect);
    assert.throws(() => functions.mixed_operation(0, 0.0, 'x', 0), expect);
  } finally {
    lib.close();
  }
});

test('lib.getFunctions() with no arguments wraps every cached function', () => {
  // Regression: the no-args branch previously returned raw native functions
  // whose shared buffer was uninitialized, producing garbage numeric results.
  // Mix SB-eligible signatures with one that is not (`string_length` takes a
  // string, which bypasses the fast path) so the no-args branch has to walk
  // the early-return path in `wrapWithSharedBuffer` alongside the wrapped
  // branch.
  const { lib } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
    add_f64: { result: 'f64', parameters: ['f64', 'f64'] },
    mixed_operation: { parameters: ['i32', 'f32', 'f64', 'u32'], result: 'f64' },
    identity_pointer: { result: 'pointer', parameters: ['pointer'] },
    string_length: { result: 'u64', parameters: ['string'] },
  });

  try {
    const all = lib.getFunctions();
    assert.strictEqual(Object.getPrototypeOf(all), null);

    // SB-eligible entries go through the shared-buffer wrapper.
    assert.strictEqual(all.add_i32(20, 22), 42);
    assert.strictEqual(all.add_f64(1.5, 2.5), 4.0);
    assert.strictEqual(all.identity_pointer(0x42n), 0x42n);

    // Non-eligible entry returns its raw native wrapper unchanged; it still
    // has to be callable from the object returned by `getFunctions()`.
    assert.strictEqual(all.string_length('hello'), 5n);

    assert.deepStrictEqual(
      Object.keys(all).sort(),
      ['add_f64', 'add_i32', 'identity_pointer',
       'mixed_operation', 'string_length']);

    assert.throws(() => all.add_i32(1), { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => all.add_i32(1.5, 0), { code: 'ERR_INVALID_ARG_VALUE' });

    assert.strictEqual(typeof all.add_i32.pointer, 'bigint');
    assert.ok(all.add_i32.pointer !== 0n);

    // The wrapper object is no longer frozen; nothing in the SB design
    // requires it.
    assert.ok(!Object.isFrozen(all));
  } finally {
    lib.close();
  }
});

test('mixed pointer + numeric signature uses the pointer-dispatch wrapper', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    call_int_callback: { result: 'i32', parameters: ['pointer', 'i32'] },
  });

  try {
    const cb = lib.registerCallback(
      { result: 'i32', parameters: ['i32'] },
      (x) => x * 2,
    );
    try {
      assert.strictEqual(functions.call_int_callback(cb, 7), 14);
      // Negative i32 must land in the numeric writer (not the pointer writer,
      // which would reject a negative BigInt).
      assert.strictEqual(functions.call_int_callback(cb, -5), -10);
    } finally {
      lib.unregisterCallback(cb);
    }
  } finally {
    lib.close();
  }
});
