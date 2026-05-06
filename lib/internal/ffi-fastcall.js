'use strict';

const {
  ArrayPrototypePush,
  Error,
  FunctionPrototypeCall,
  NumberIsInteger,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectKeys,
  ReflectApply,
  TypeError,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_FFI_LIBRARY_CLOSED,
    ERR_INTERNAL_ASSERTION,
  },
} = require('internal/errors');

const ffiBinding = internalBinding('ffi');
const {
  DynamicLibrary,
  charIsSigned,
  uintptrMax,
} = ffiBinding;

const kFastcallAlive = ffiBinding.kFastcallAlive;
const kFastcallInvokeSlow = ffiBinding.kFastcallInvokeSlow;
const kFastcallParams = ffiBinding.kFastcallParams;
const kFastcallResult = ffiBinding.kFastcallResult;

const fastcallEnabled = kFastcallAlive !== undefined;

const U64_MAX = 0xFFFFFFFFFFFFFFFFn;
const I64_MAX = 0x7FFFFFFFFFFFFFFFn;
const I64_MIN = -0x8000000000000000n;

// Note: TYPES (the outer object) has __proto__: null because user-input
// type names are looked up here (e.g., TYPES[parameters[i]]), so the
// outer guard prevents prototype-chain probing. The inner entries
// deliberately do NOT have __proto__: null. The hot path reads
// info.kind / info.min / info.max / info.label from these objects, and
// V8's inline caches dispatch ~50% faster on Object-prototype-inheriting
// shapes than on null-proto ones across this dispatch pattern. The
// inner objects are never user-keyed, so the safety concern doesn't
// apply. See commit 53bf0595a1f for the regression analysis.
const TYPES = {
  __proto__: null,
  i8:    { kind: 'int', min: -128, max: 127, label: 'an int8' },
  int8:  { kind: 'int', min: -128, max: 127, label: 'an int8' },
  char:  charIsSigned
    ? { kind: 'int', min: -128, max: 127, label: 'an int8' }
    : { kind: 'int', min: 0, max: 255, label: 'a uint8' },
  u8:    { kind: 'int', min: 0, max: 255, label: 'a uint8' },
  uint8: { kind: 'int', min: 0, max: 255, label: 'a uint8' },
  bool:  { kind: 'int', min: 0, max: 255, label: 'a uint8' },
  i16:   { kind: 'int', min: -32768, max: 32767, label: 'an int16' },
  int16: { kind: 'int', min: -32768, max: 32767, label: 'an int16' },
  u16:   { kind: 'int', min: 0, max: 65535, label: 'a uint16' },
  uint16: { kind: 'int', min: 0, max: 65535, label: 'a uint16' },
  i32:   { kind: 'int32', label: 'an int32' },
  int32: { kind: 'int32', label: 'an int32' },
  u32:   { kind: 'uint32', label: 'a uint32' },
  uint32: { kind: 'uint32', label: 'a uint32' },
  i64:   { kind: 'i64', label: 'an int64' },
  int64: { kind: 'i64', label: 'an int64' },
  u64:   { kind: 'u64', label: 'a uint64' },
  uint64: { kind: 'u64', label: 'a uint64' },
  f32:   { kind: 'float', label: 'a float' },
  float: { kind: 'float', label: 'a float' },
  float32: { kind: 'float', label: 'a float' },
  f64:   { kind: 'double', label: 'a double' },
  double: { kind: 'double', label: 'a double' },
  float64: { kind: 'double', label: 'a double' },
  pointer: { kind: 'pointer' },
  ptr:   { kind: 'pointer' },
  buffer: { kind: 'pointer' },
  arraybuffer: { kind: 'pointer' },
  string: { kind: 'pointer' },
  str:   { kind: 'pointer' },
};

function throwArg(msg) {
  // eslint-disable-next-line no-restricted-syntax
  const err = new TypeError(msg);
  err.code = 'ERR_INVALID_ARG_VALUE';
  throw err;
}

// Per-kind specialized validators. Picked once at wrapper-build time, so
// the hot path doesn't re-dispatch on info.kind per call. Each returns the
// arg unchanged on success.
//
// i32/u32 use the bitwise-coerce-and-compare idiom. `(a | 0)` first ToInt32-
// coerces a (NaN/Infinity → 0, non-numbers → 0, fractional → trunc, out-of-
// range → wrap mod 2^32 with sign extension); the `=== a` then rejects every
// case where the coercion changed the value. Same logic for `>>> 0` against
// uint32. This collapses what would otherwise be typeof + NumberIsInteger +
// two range comparisons into one branch.
function validateI32(arg, idx) {
  if ((arg | 0) !== arg) throwArg(`Argument ${idx} must be an int32`);
  return arg;
}
function validateU32(arg, idx) {
  if ((arg >>> 0) !== arg) throwArg(`Argument ${idx} must be a uint32`);
  return arg;
}
function validateI64(arg, idx) {
  if (typeof arg !== 'bigint' || arg < I64_MIN || arg > I64_MAX) {
    throwArg(`Argument ${idx} must be an int64`);
  }
  return arg;
}
function validateU64(arg, idx) {
  if (typeof arg !== 'bigint' || arg < 0n || arg > U64_MAX) {
    throwArg(`Argument ${idx} must be a uint64`);
  }
  return arg;
}
function validateF32(arg, idx) {
  if (typeof arg !== 'number') throwArg(`Argument ${idx} must be a float`);
  return arg;
}
function validateF64(arg, idx) {
  if (typeof arg !== 'number') throwArg(`Argument ${idx} must be a double`);
  return arg;
}

// Generic validator for narrow ints (i8/u8/i16/u16/bool/char) — used by the
// full wrapper, never by the strict-numeric one. The narrower-int range is
// per-type so it stays generic.
function validateAndCoerce(info, arg, idx) {
  const k = info.kind;
  if (k === 'int') {
    if (typeof arg !== 'number' || !NumberIsInteger(arg) ||
        arg < info.min || arg > info.max) {
      throwArg(`Argument ${idx} must be ${info.label}`);
    }
    return arg;
  }
  if (k === 'int32') return validateI32(arg, idx);
  if (k === 'uint32') return validateU32(arg, idx);
  if (k === 'i64') return validateI64(arg, idx);
  if (k === 'u64') return validateU64(arg, idx);
  if (k === 'float') return validateF32(arg, idx);
  if (k === 'double') return validateF64(arg, idx);
  throw new ERR_INTERNAL_ASSERTION(
    `FFI: unexpected kind="${k}" in validateAndCoerce`);
}

function validatorFor(info) {
  switch (info.kind) {
    case 'int32': return validateI32;
    case 'uint32': return validateU32;
    case 'i64': return validateI64;
    case 'u64': return validateU64;
    case 'float': return validateF32;
    case 'double': return validateF64;
    default:
      throw new ERR_INTERNAL_ASSERTION(
        `FFI: validatorFor called with non-strict-numeric kind "${info.kind}"`);
  }
}

const SLOW_PATH = Symbol('slow-path');
function coercePointer(arg, idx) {
  if (typeof arg === 'bigint') {
    if (arg < 0n || arg > uintptrMax) {
      throwArg(`Argument ${idx} must be a non-negative pointer bigint`);
    }
    return arg;
  }
  if (arg === null || arg === undefined) return 0n;
  return SLOW_PATH;
}

// Specialized wrapper for strict-numeric signatures (no pointers). Each
// argument is validated by a per-kind validator picked once at build time
// — V8 inline-caches the call to the specific function, the kind branch
// is gone from the hot path, and i32/u32 use the `(a | 0) === a` and
// `(a >>> 0) === a` idioms (see comment on validateI32 above).
//
// The wrapper still validates every arg because V8's fast-call coercion
// silently truncates non-integers and out-of-range values when the call
// site is fully optimized (CheckedNumberAsWord32 wraps mod 2^32, Checked-
// BigIntTruncatingWord64 wraps to low 64 bits). Without the JS check the
// same FFI binding would throw cold and silently corrupt hot.
function buildStrictNumericWrapper(rawFn, alive, infos, nargs) {
  switch (nargs) {
    case 0:
      return function() {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 0) {
          throwArg(`Invalid argument count: expected 0, got ${arguments.length}`);
        }
        return rawFn();
      };
    case 1: {
      const v0 = validatorFor(infos[0]);
      return function(a0) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 1) {
          throwArg(`Invalid argument count: expected 1, got ${arguments.length}`);
        }
        return rawFn(v0(a0, 0));
      };
    }
    case 2: {
      const v0 = validatorFor(infos[0]); const v1 = validatorFor(infos[1]);
      return function(a0, a1) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 2) {
          throwArg(`Invalid argument count: expected 2, got ${arguments.length}`);
        }
        return rawFn(v0(a0, 0), v1(a1, 1));
      };
    }
    case 3: {
      const v0 = validatorFor(infos[0]); const v1 = validatorFor(infos[1]);
      const v2 = validatorFor(infos[2]);
      return function(a0, a1, a2) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 3) {
          throwArg(`Invalid argument count: expected 3, got ${arguments.length}`);
        }
        return rawFn(v0(a0, 0), v1(a1, 1), v2(a2, 2));
      };
    }
    case 4: {
      const v0 = validatorFor(infos[0]); const v1 = validatorFor(infos[1]);
      const v2 = validatorFor(infos[2]); const v3 = validatorFor(infos[3]);
      return function(a0, a1, a2, a3) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 4) {
          throwArg(`Invalid argument count: expected 4, got ${arguments.length}`);
        }
        return rawFn(v0(a0, 0), v1(a1, 1), v2(a2, 2), v3(a3, 3));
      };
    }
    case 5: {
      const v0 = validatorFor(infos[0]); const v1 = validatorFor(infos[1]);
      const v2 = validatorFor(infos[2]); const v3 = validatorFor(infos[3]);
      const v4 = validatorFor(infos[4]);
      return function(a0, a1, a2, a3, a4) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 5) {
          throwArg(`Invalid argument count: expected 5, got ${arguments.length}`);
        }
        return rawFn(v0(a0, 0), v1(a1, 1), v2(a2, 2), v3(a3, 3), v4(a4, 4));
      };
    }
    case 6: {
      const v0 = validatorFor(infos[0]); const v1 = validatorFor(infos[1]);
      const v2 = validatorFor(infos[2]); const v3 = validatorFor(infos[3]);
      const v4 = validatorFor(infos[4]); const v5 = validatorFor(infos[5]);
      return function(a0, a1, a2, a3, a4, a5) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 6) {
          throwArg(`Invalid argument count: expected 6, got ${arguments.length}`);
        }
        return rawFn(v0(a0, 0), v1(a1, 1), v2(a2, 2), v3(a3, 3),
                     v4(a4, 4), v5(a5, 5));
      };
    }
    default: {
      const validators = [];
      for (let i = 0; i < nargs; i++) {
        ArrayPrototypePush(validators, validatorFor(infos[i]));
      }
      return function(...args) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (args.length !== nargs) {
          throwArg(`Invalid argument count: expected ${nargs}, got ${args.length}`);
        }
        const out = [];
        for (let i = 0; i < nargs; i++) {
          ArrayPrototypePush(out, validators[i](args[i], i));
        }
        return ReflectApply(rawFn, undefined, out);
      };
    }
  }
}

function inheritMetadata(wrapper, rawFn, nargs) {
  ObjectDefineProperty(wrapper, 'name', {
    __proto__: null, value: rawFn.name, configurable: true,
  });
  ObjectDefineProperty(wrapper, 'length', {
    __proto__: null, value: nargs, configurable: true,
  });
  ObjectDefineProperty(wrapper, 'pointer', {
    __proto__: null, value: rawFn.pointer,
    writable: true, configurable: true, enumerable: true,
  });
  return wrapper;
}

// Types eligible for the strict-numeric wrapper: per-arg validation runs
// the same way as the full wrapper, but the dispatch is simpler (no pointer
// branches, smaller closure). We don't include narrower ints (i8/u8/i16/u16/
// bool/char) because their range check is per-type rather than per-kind,
// and they're a minority of fast-call signatures — the full wrapper already
// handles them. Pointer types are excluded because they need null/undefined
// → 0n coercion plus object-arg slow-path fallback.
const STRICT_NUMERIC_TYPES = new Set([
  'i32', 'int32', 'u32', 'uint32',
  'i64', 'int64', 'u64', 'uint64',
  'f32', 'float', 'float32',
  'f64', 'double', 'float64',
]);

function isStrictNumericSignature(parameters, resultType) {
  if (resultType !== 'void' && !STRICT_NUMERIC_TYPES.has(resultType)) {
    return false;
  }
  for (let i = 0; i < parameters.length; i++) {
    if (!STRICT_NUMERIC_TYPES.has(parameters[i])) return false;
  }
  return true;
}

function buildWrapper(rawFn, parameters, resultType) {
  if (!fastcallEnabled) return rawFn;
  if (rawFn === undefined || rawFn === null) return rawFn;
  const aliveAB = rawFn[kFastcallAlive];
  if (aliveAB === undefined) return rawFn;

  const slowInvoke = rawFn[kFastcallInvokeSlow];
  if (parameters === undefined) parameters = rawFn[kFastcallParams];
  if (resultType === undefined) resultType = rawFn[kFastcallResult];
  if (parameters === undefined || resultType === undefined ||
      slowInvoke === undefined) {
    throw new ERR_INTERNAL_ASSERTION(
      'FFI: fast-call raw function is missing required metadata');
  }

  const alive = new Uint8Array(aliveAB);
  const nargs = parameters.length;

  const infos = [];
  const isPointer = [];
  for (let i = 0; i < nargs; i++) {
    const info = TYPES[parameters[i]];
    if (info === undefined) {
      throw new ERR_INTERNAL_ASSERTION(
        `FFI: fast-call type table missing entry for "${parameters[i]}"`);
    }
    infos.push(info);
    isPointer.push(info.kind === 'pointer');
  }

  // Fast path: pure-numeric signatures get a wrapper without per-arg pointer
  // branches or slow-path closure capture. Same per-arg validation as the
  // full wrapper.
  if (isStrictNumericSignature(parameters, resultType)) {
    return inheritMetadata(
      buildStrictNumericWrapper(rawFn, alive, infos, nargs), rawFn, nargs);
  }

  let wrapper;
  switch (nargs) {
    case 0:
      wrapper = function() {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 0) {
          throwArg(`Invalid argument count: expected 0, got ${arguments.length}`);
        }
        return rawFn();
      };
      break;
    case 1: {
      const i0 = infos[0];
      const p0 = isPointer[0];
      wrapper = function(a0) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 1) {
          throwArg(`Invalid argument count: expected 1, got ${arguments.length}`);
        }
        let v0;
        if (p0) {
          v0 = coercePointer(a0, 0);
          if (v0 === SLOW_PATH) return slowInvoke(a0);
        } else {
          v0 = validateAndCoerce(i0, a0, 0);
        }
        return rawFn(v0);
      };
      break;
    }
    case 2: {
      const i0 = infos[0]; const i1 = infos[1];
      const p0 = isPointer[0]; const p1 = isPointer[1];
      wrapper = function(a0, a1) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 2) {
          throwArg(`Invalid argument count: expected 2, got ${arguments.length}`);
        }
        let v0, v1;
        if (p0) {
          v0 = coercePointer(a0, 0);
          if (v0 === SLOW_PATH) return slowInvoke(a0, a1);
        } else {
          v0 = validateAndCoerce(i0, a0, 0);
        }
        if (p1) {
          v1 = coercePointer(a1, 1);
          if (v1 === SLOW_PATH) return slowInvoke(a0, a1);
        } else {
          v1 = validateAndCoerce(i1, a1, 1);
        }
        return rawFn(v0, v1);
      };
      break;
    }
    case 3: {
      const i0 = infos[0]; const i1 = infos[1]; const i2 = infos[2];
      const p0 = isPointer[0]; const p1 = isPointer[1]; const p2 = isPointer[2];
      wrapper = function(a0, a1, a2) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 3) {
          throwArg(`Invalid argument count: expected 3, got ${arguments.length}`);
        }
        let v0, v1, v2;
        if (p0) { v0 = coercePointer(a0, 0); if (v0 === SLOW_PATH) return slowInvoke(a0, a1, a2); }
        else    { v0 = validateAndCoerce(i0, a0, 0); }
        if (p1) { v1 = coercePointer(a1, 1); if (v1 === SLOW_PATH) return slowInvoke(a0, a1, a2); }
        else    { v1 = validateAndCoerce(i1, a1, 1); }
        if (p2) { v2 = coercePointer(a2, 2); if (v2 === SLOW_PATH) return slowInvoke(a0, a1, a2); }
        else    { v2 = validateAndCoerce(i2, a2, 2); }
        return rawFn(v0, v1, v2);
      };
      break;
    }
    case 4: {
      const i0 = infos[0]; const i1 = infos[1]; const i2 = infos[2]; const i3 = infos[3];
      const p0 = isPointer[0]; const p1 = isPointer[1]; const p2 = isPointer[2]; const p3 = isPointer[3];
      wrapper = function(a0, a1, a2, a3) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 4) {
          throwArg(`Invalid argument count: expected 4, got ${arguments.length}`);
        }
        let v0, v1, v2, v3;
        if (p0) { v0 = coercePointer(a0, 0); if (v0 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3); }
        else    { v0 = validateAndCoerce(i0, a0, 0); }
        if (p1) { v1 = coercePointer(a1, 1); if (v1 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3); }
        else    { v1 = validateAndCoerce(i1, a1, 1); }
        if (p2) { v2 = coercePointer(a2, 2); if (v2 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3); }
        else    { v2 = validateAndCoerce(i2, a2, 2); }
        if (p3) { v3 = coercePointer(a3, 3); if (v3 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3); }
        else    { v3 = validateAndCoerce(i3, a3, 3); }
        return rawFn(v0, v1, v2, v3);
      };
      break;
    }
    case 5: {
      const i0 = infos[0]; const i1 = infos[1]; const i2 = infos[2]; const i3 = infos[3]; const i4 = infos[4];
      const p0 = isPointer[0]; const p1 = isPointer[1]; const p2 = isPointer[2]; const p3 = isPointer[3]; const p4 = isPointer[4];
      wrapper = function(a0, a1, a2, a3, a4) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 5) {
          throwArg(`Invalid argument count: expected 5, got ${arguments.length}`);
        }
        let v0, v1, v2, v3, v4;
        if (p0) { v0 = coercePointer(a0, 0); if (v0 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4); }
        else    { v0 = validateAndCoerce(i0, a0, 0); }
        if (p1) { v1 = coercePointer(a1, 1); if (v1 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4); }
        else    { v1 = validateAndCoerce(i1, a1, 1); }
        if (p2) { v2 = coercePointer(a2, 2); if (v2 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4); }
        else    { v2 = validateAndCoerce(i2, a2, 2); }
        if (p3) { v3 = coercePointer(a3, 3); if (v3 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4); }
        else    { v3 = validateAndCoerce(i3, a3, 3); }
        if (p4) { v4 = coercePointer(a4, 4); if (v4 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4); }
        else    { v4 = validateAndCoerce(i4, a4, 4); }
        return rawFn(v0, v1, v2, v3, v4);
      };
      break;
    }
    case 6: {
      const i0 = infos[0]; const i1 = infos[1]; const i2 = infos[2]; const i3 = infos[3]; const i4 = infos[4]; const i5 = infos[5];
      const p0 = isPointer[0]; const p1 = isPointer[1]; const p2 = isPointer[2]; const p3 = isPointer[3]; const p4 = isPointer[4]; const p5 = isPointer[5];
      wrapper = function(a0, a1, a2, a3, a4, a5) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (arguments.length !== 6) {
          throwArg(`Invalid argument count: expected 6, got ${arguments.length}`);
        }
        let v0, v1, v2, v3, v4, v5;
        if (p0) { v0 = coercePointer(a0, 0); if (v0 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4, a5); }
        else    { v0 = validateAndCoerce(i0, a0, 0); }
        if (p1) { v1 = coercePointer(a1, 1); if (v1 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4, a5); }
        else    { v1 = validateAndCoerce(i1, a1, 1); }
        if (p2) { v2 = coercePointer(a2, 2); if (v2 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4, a5); }
        else    { v2 = validateAndCoerce(i2, a2, 2); }
        if (p3) { v3 = coercePointer(a3, 3); if (v3 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4, a5); }
        else    { v3 = validateAndCoerce(i3, a3, 3); }
        if (p4) { v4 = coercePointer(a4, 4); if (v4 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4, a5); }
        else    { v4 = validateAndCoerce(i4, a4, 4); }
        if (p5) { v5 = coercePointer(a5, 5); if (v5 === SLOW_PATH) return slowInvoke(a0, a1, a2, a3, a4, a5); }
        else    { v5 = validateAndCoerce(i5, a5, 5); }
        return rawFn(v0, v1, v2, v3, v4, v5);
      };
      break;
    }
    default:
      // 7+ args: per-call array allocation is unavoidable in the rest-args
      // path. Specializing further is diminishing returns; large-arity FFI
      // calls are rare.
      wrapper = function(...args) {
        if (alive[0] !== 0) throw new ERR_FFI_LIBRARY_CLOSED();
        if (args.length !== nargs) {
          throwArg(`Invalid argument count: expected ${nargs}, got ${args.length}`);
        }
        const out = [];
        for (let i = 0; i < nargs; i++) {
          if (isPointer[i]) {
            const v = coercePointer(args[i], i);
            if (v === SLOW_PATH) {
              return ReflectApply(slowInvoke, undefined, args);
            }
            ArrayPrototypePush(out, v);
          } else {
            ArrayPrototypePush(out, validateAndCoerce(infos[i], args[i], i));
          }
        }
        return ReflectApply(rawFn, undefined, out);
      };
      break;
  }
  return inheritMetadata(wrapper, rawFn, nargs);
}

function sigParams(sig) {
  return sig.parameters ?? sig.arguments ?? [];
}
function sigResult(sig) {
  return sig.result ?? sig.return ?? sig.returns ?? 'void';
}

const rawGetFunction = DynamicLibrary.prototype.getFunction;
const rawGetFunctions = DynamicLibrary.prototype.getFunctions;

DynamicLibrary.prototype.getFunction = function getFunction(name, sig) {
  const raw = FunctionPrototypeCall(rawGetFunction, this, name, sig);
  return buildWrapper(raw, sigParams(sig), sigResult(sig));
};

DynamicLibrary.prototype.getFunctions = function getFunctions(definitions) {
  const raw = definitions === undefined ?
    FunctionPrototypeCall(rawGetFunctions, this) :
    FunctionPrototypeCall(rawGetFunctions, this, definitions);
  if (raw === undefined || raw === null) return raw;
  const keys = ObjectKeys(raw);
  const out = { __proto__: null };
  for (let i = 0; i < keys.length; i++) {
    const name = keys[i];
    if (definitions === undefined) {
      out[name] = buildWrapper(raw[name]);
    } else {
      const sig = definitions[name];
      out[name] = buildWrapper(raw[name], sigParams(sig), sigResult(sig));
    }
  }
  return out;
};

{
  const desc = ObjectGetOwnPropertyDescriptor(
    DynamicLibrary.prototype, 'functions');
  if (desc === undefined || !desc.get) {
    throw new ERR_INTERNAL_ASSERTION(
      'FFI: DynamicLibrary.prototype.functions accessor not found');
  }
  const origGetter = desc.get;
  ObjectDefineProperty(DynamicLibrary.prototype, 'functions', {
    __proto__: null,
    configurable: true,
    enumerable: desc.enumerable,
    get() {
      const raw = FunctionPrototypeCall(origGetter, this);
      if (raw === undefined || raw === null) return raw;
      const wrapped = { __proto__: null };
      const keys = ObjectKeys(raw);
      for (let i = 0; i < keys.length; i++) {
        const name = keys[i];
        wrapped[name] = buildWrapper(raw[name]);
      }
      return wrapped;
    },
  });
}

module.exports = { buildWrapper };
