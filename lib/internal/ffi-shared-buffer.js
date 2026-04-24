'use strict';

const {
  DataView,
  DataViewPrototypeGetBigInt64,
  DataViewPrototypeGetBigUint64,
  DataViewPrototypeGetFloat32,
  DataViewPrototypeGetFloat64,
  DataViewPrototypeGetInt16,
  DataViewPrototypeGetInt32,
  DataViewPrototypeGetInt8,
  DataViewPrototypeGetUint16,
  DataViewPrototypeGetUint32,
  DataViewPrototypeGetUint8,
  DataViewPrototypeSetBigInt64,
  DataViewPrototypeSetBigUint64,
  DataViewPrototypeSetFloat32,
  DataViewPrototypeSetFloat64,
  DataViewPrototypeSetInt16,
  DataViewPrototypeSetInt32,
  DataViewPrototypeSetInt8,
  DataViewPrototypeSetUint16,
  DataViewPrototypeSetUint32,
  DataViewPrototypeSetUint8,
  FunctionPrototypeCall,
  NumberIsInteger,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectKeys,
  ReflectApply,
  TypeError,
} = primordials;

const {
  codes: {
    ERR_INTERNAL_ASSERTION,
  },
} = require('internal/errors');

const {
  DynamicLibrary,
  charIsSigned,
  kSbInvokeSlow,
  kSbParams,
  kSbResult,
  kSbSharedBuffer,
  uintptrMax,
} = internalBinding('ffi');

// Validator fields (`min`, `max`, `label`) must mirror `ToFFIArgument` in
// `src/ffi/types.cc` so the fast and slow paths produce identical errors.
const sI8 = DataViewPrototypeSetInt8;
const gI8 = DataViewPrototypeGetInt8;
const sU8 = DataViewPrototypeSetUint8;
const gU8 = DataViewPrototypeGetUint8;
const sI16 = DataViewPrototypeSetInt16;
const gI16 = DataViewPrototypeGetInt16;
const sU16 = DataViewPrototypeSetUint16;
const gU16 = DataViewPrototypeGetUint16;
const sI32 = DataViewPrototypeSetInt32;
const gI32 = DataViewPrototypeGetInt32;
const sU32 = DataViewPrototypeSetUint32;
const gU32 = DataViewPrototypeGetUint32;
const sI64 = DataViewPrototypeSetBigInt64;
const gI64 = DataViewPrototypeGetBigInt64;
const sU64 = DataViewPrototypeSetBigUint64;
const gU64 = DataViewPrototypeGetBigUint64;
const sF32 = DataViewPrototypeSetFloat32;
const gF32 = DataViewPrototypeGetFloat32;
const sF64 = DataViewPrototypeSetFloat64;
const gF64 = DataViewPrototypeGetFloat64;

const sbTypeInfo = {
  __proto__: null,
  i8: { set: sI8, get: gI8, kind: 'int', min: -128, max: 127, label: 'an int8' },
  int8: { set: sI8, get: gI8, kind: 'int', min: -128, max: 127, label: 'an int8' },
  char: charIsSigned ?
    { set: sI8, get: gI8, kind: 'int', min: -128, max: 127, label: 'an int8' } :
    { set: sU8, get: gU8, kind: 'int', min: 0, max: 255, label: 'a uint8' },
  u8: { set: sU8, get: gU8, kind: 'int', min: 0, max: 255, label: 'a uint8' },
  uint8: { set: sU8, get: gU8, kind: 'int', min: 0, max: 255, label: 'a uint8' },
  bool: { set: sU8, get: gU8, kind: 'int', min: 0, max: 255, label: 'a uint8' },
  i16: { set: sI16, get: gI16, kind: 'int', min: -32768, max: 32767, label: 'an int16' },
  int16: { set: sI16, get: gI16, kind: 'int', min: -32768, max: 32767, label: 'an int16' },
  u16: { set: sU16, get: gU16, kind: 'int', min: 0, max: 65535, label: 'a uint16' },
  uint16: { set: sU16, get: gU16, kind: 'int', min: 0, max: 65535, label: 'a uint16' },
  i32: { set: sI32, get: gI32, kind: 'int', min: -2147483648, max: 2147483647, label: 'an int32' },
  int32: { set: sI32, get: gI32, kind: 'int', min: -2147483648, max: 2147483647, label: 'an int32' },
  u32: { set: sU32, get: gU32, kind: 'int', min: 0, max: 4294967295, label: 'a uint32' },
  uint32: { set: sU32, get: gU32, kind: 'int', min: 0, max: 4294967295, label: 'a uint32' },
  i64: { set: sI64, get: gI64, kind: 'i64', label: 'an int64' },
  int64: { set: sI64, get: gI64, kind: 'i64', label: 'an int64' },
  u64: { set: sU64, get: gU64, kind: 'u64', label: 'a uint64' },
  uint64: { set: sU64, get: gU64, kind: 'u64', label: 'a uint64' },
  f32: { set: sF32, get: gF32, kind: 'float', label: 'a float' },
  float: { set: sF32, get: gF32, kind: 'float', label: 'a float' },
  float32: { set: sF32, get: gF32, kind: 'float', label: 'a float' },
  f64: { set: sF64, get: gF64, kind: 'float', label: 'a double' },
  double: { set: sF64, get: gF64, kind: 'float', label: 'a double' },
  float64: { set: sF64, get: gF64, kind: 'float', label: 'a double' },
  pointer: { set: sU64, get: gU64, kind: 'pointer' },
  ptr: { set: sU64, get: gU64, kind: 'pointer' },
  function: { set: sU64, get: gU64, kind: 'pointer' },
  buffer: { set: sU64, get: gU64, kind: 'pointer' },
  arraybuffer: { set: sU64, get: gU64, kind: 'pointer' },
  string: { set: sU64, get: gU64, kind: 'pointer' },
  str: { set: sU64, get: gU64, kind: 'pointer' },
};

const U64_MAX = 0xFFFFFFFFFFFFFFFFn;
const I64_MAX = 0x7FFFFFFFFFFFFFFFn;
const I64_MIN = -0x8000000000000000n;

// Builds the exact error shape the non-SB implementation (the native FFI
// invoker) produces.
function throwFFIArgError(msg) {
  // eslint-disable-next-line no-restricted-syntax
  const err = new TypeError(msg);
  err.code = 'ERR_INVALID_ARG_VALUE';
  throw err;
}

function throwFFIArgCountError(expected, actual) {
  throwFFIArgError(
    `Invalid argument count: expected ${expected}, got ${actual}`);
}

// Validation and error messages must mirror `ToFFIArgument` in
// `src/ffi/types.cc`.
function writeNumericArg(view, info, offset, arg, index) {
  const kind = info.kind;
  if (kind === 'int') {
    if (typeof arg !== 'number' || !NumberIsInteger(arg) ||
        arg < info.min || arg > info.max) {
      throwFFIArgError(`Argument ${index} must be ${info.label}`);
    }
    info.set(view, offset, arg, true);
    return;
  }
  if (kind === 'i64') {
    if (typeof arg !== 'bigint' || arg < I64_MIN || arg > I64_MAX) {
      throwFFIArgError(`Argument ${index} must be ${info.label}`);
    }
    sI64(view, offset, arg, true);
    return;
  }
  if (kind === 'u64') {
    if (typeof arg !== 'bigint' || arg < 0n || arg > U64_MAX) {
      throwFFIArgError(`Argument ${index} must be ${info.label}`);
    }
    sU64(view, offset, arg, true);
    return;
  }
  if (kind === 'float') {
    if (typeof arg !== 'number') {
      throwFFIArgError(`Argument ${index} must be ${info.label}`);
    }
    info.set(view, offset, arg, true);
    return;
  }

  /* c8 ignore start */
  // Unreachable: caller filters out non-numeric kinds.
  throw new ERR_INTERNAL_ASSERTION(
    `FFI: writeNumericArg reached with unexpected kind="${kind}"`);
  /* c8 ignore stop */
}

// Returns true on fast-path success, false when the caller must fall back
// to the slow path (strings, Buffers, ArrayBuffers, ArrayBufferViews).
function writePointerArg(view, offset, arg, index) {
  if (typeof arg === 'bigint') {
    // Bound by `uintptrMax` (not `U64_MAX`) to mirror the slow path: on
    // 32-bit platforms a BigInt that doesn't fit in `uintptr_t` would be
    // silently truncated by `ReadFFIArgFromBuffer`'s
    // `memcpy(..., type->size, ...)`.
    if (arg < 0n || arg > uintptrMax) {
      throwFFIArgError(
        `Argument ${index} must be a non-negative pointer bigint`);
    }
    sU64(view, offset, arg, true);
    return true;
  }
  if (arg === null || arg === undefined) {
    sU64(view, offset, 0n, true);
    return true;
  }
  return false;
}

// The `pointer` descriptor mirrors the raw function's so user code that
// reassigns `.pointer` keeps working through the wrapper.
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

// Reentrancy: the ArrayBuffer is per-function, but `InvokeFunctionSB` copies
// arguments out of it into invocation-local storage before `ffi_call` and
// reads the return value back only after, so nested/reentrant calls into
// the same function are safe.
function wrapWithSharedBuffer(rawFn, parameters, resultType) {
  if (rawFn === undefined || rawFn === null) return rawFn;
  const buffer = rawFn[kSbSharedBuffer];
  if (buffer === undefined) return rawFn;

  // Callers without explicit signature info (the `functions` accessor
  // patch below) rely on the `kSbParams` / `kSbResult` metadata attached
  // by the native `CreateFunction`.
  if (parameters === undefined) parameters = rawFn[kSbParams];
  if (resultType === undefined) resultType = rawFn[kSbResult];
  // `CreateFunction` always attaches these for SB-eligible functions.
  // Missing here means the native side and this wrapper are out of sync.
  /* c8 ignore start */
  if (parameters === undefined || resultType === undefined) {
    throw new ERR_INTERNAL_ASSERTION(
      'FFI: shared-buffer raw function is missing kSbParams or kSbResult');
  }
  /* c8 ignore stop */

  const slowInvoke = rawFn[kSbInvokeSlow];
  const view = new DataView(buffer);
  let retGetter = null;
  if (resultType !== 'void') {
    const retInfo = sbTypeInfo[resultType];
    /* c8 ignore start */
    if (retInfo === undefined) {
      throw new ERR_INTERNAL_ASSERTION(
        `FFI: shared-buffer type table missing entry for result type "${resultType}"`);
    }
    /* c8 ignore stop */
    retGetter = retInfo.get;
  }

  const nargs = parameters.length;
  const argInfos = [];
  const argOffsets = [];
  let anyPointer = false;
  for (let i = 0; i < nargs; i++) {
    const info = sbTypeInfo[parameters[i]];
    /* c8 ignore start */
    if (info === undefined) {
      throw new ERR_INTERNAL_ASSERTION(
        `FFI: shared-buffer type table missing entry for parameter type "${parameters[i]}"`);
    }
    /* c8 ignore stop */
    // Push the `sbTypeInfo` entry directly (entries with the same `kind`
    // share a shape, keeping `writeNumericArg`'s call sites
    // low-polymorphism) and store offsets in a parallel array to avoid
    // per-arg object cloning.
    argInfos.push(info);
    argOffsets.push(8 * (i + 1));
    if (info.kind === 'pointer') anyPointer = true;
  }

  let wrapper;
  if (anyPointer) {
    // Pointer signatures need a per-arg runtime type check and fall back
    // to the native slow-path invoker for non-BigInt pointer arguments,
    // so arity specialization wouldn't buy much here.
    /* c8 ignore start */
    if (slowInvoke === undefined) {
      throw new ERR_INTERNAL_ASSERTION(
        'FFI: shared-buffer raw function with pointer arguments is ' +
        'missing kSbInvokeSlow');
    }
    /* c8 ignore stop */
    wrapper = function(...args) {
      if (args.length !== nargs) {
        throwFFIArgCountError(nargs, args.length);
      }
      for (let i = 0; i < nargs; i++) {
        const info = argInfos[i];
        const offset = argOffsets[i];
        if (info.kind === 'pointer') {
          if (!writePointerArg(view, offset, args[i], i)) {
            return ReflectApply(slowInvoke, undefined, args);
          }
        } else {
          writeNumericArg(view, info, offset, args[i], i);
        }
      }
      rawFn();
      return retGetter === null ? undefined : retGetter(view, 0, true);
    };
  } else {
    // Arity specialization avoids the per-call `Array` allocation of
    // `...args`; the void/non-void split removes a per-call branch on
    // `retGetter`.
    wrapper = buildNumericWrapper(
      rawFn, view, argInfos, argOffsets, nargs, retGetter);
  }
  return inheritMetadata(wrapper, rawFn, nargs);
}

// Specialized for nargs 0..6 with a generic rest-params fallback for 7+.
// Per-arg type info and offsets are captured into closure locals so the
// hot path reads a single variable per arg. This is admittedly pretty weird
// but it's the result of lots of perf-hunting.
function buildNumericWrapper(
  rawFn, view, argInfos, argOffsets, nargs, retGetter) {
  // `IsSBEligibleSignature` on the native side rejects 0-arg signatures,
  // so this branch is unreachable today. It's kept as defense-in-depth
  // for when that filter changes or for programmatic callers that hand a
  // 0-arg signature through `wrapWithSharedBuffer` directly.
  /* c8 ignore start */
  if (nargs === 0) {
    if (retGetter === null) {
      return function() {
        if (arguments.length !== 0) {
          throwFFIArgCountError(0, arguments.length);
        }
        rawFn();
      };
    }
    return function() {
      if (arguments.length !== 0) {
        throwFFIArgCountError(0, arguments.length);
      }
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  /* c8 ignore stop */
  if (nargs === 1) {
    const i0 = argInfos[0];
    const o0 = argOffsets[0];
    if (retGetter === null) {
      return function(a0) {
        if (arguments.length !== 1) {
          throwFFIArgCountError(1, arguments.length);
        }
        writeNumericArg(view, i0, o0, a0, 0);
        rawFn();
      };
    }
    return function(a0) {
      if (arguments.length !== 1) {
        throwFFIArgCountError(1, arguments.length);
      }
      writeNumericArg(view, i0, o0, a0, 0);
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  if (nargs === 2) {
    const i0 = argInfos[0];
    const i1 = argInfos[1];
    const o0 = argOffsets[0];
    const o1 = argOffsets[1];
    if (retGetter === null) {
      return function(a0, a1) {
        if (arguments.length !== 2) {
          throwFFIArgCountError(2, arguments.length);
        }
        writeNumericArg(view, i0, o0, a0, 0);
        writeNumericArg(view, i1, o1, a1, 1);
        rawFn();
      };
    }
    return function(a0, a1) {
      if (arguments.length !== 2) {
        throwFFIArgCountError(2, arguments.length);
      }
      writeNumericArg(view, i0, o0, a0, 0);
      writeNumericArg(view, i1, o1, a1, 1);
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  if (nargs === 3) {
    const i0 = argInfos[0];
    const i1 = argInfos[1];
    const i2 = argInfos[2];
    const o0 = argOffsets[0];
    const o1 = argOffsets[1];
    const o2 = argOffsets[2];
    if (retGetter === null) {
      return function(a0, a1, a2) {
        if (arguments.length !== 3) {
          throwFFIArgCountError(3, arguments.length);
        }
        writeNumericArg(view, i0, o0, a0, 0);
        writeNumericArg(view, i1, o1, a1, 1);
        writeNumericArg(view, i2, o2, a2, 2);
        rawFn();
      };
    }
    return function(a0, a1, a2) {
      if (arguments.length !== 3) {
        throwFFIArgCountError(3, arguments.length);
      }
      writeNumericArg(view, i0, o0, a0, 0);
      writeNumericArg(view, i1, o1, a1, 1);
      writeNumericArg(view, i2, o2, a2, 2);
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  if (nargs === 4) {
    const i0 = argInfos[0];
    const i1 = argInfos[1];
    const i2 = argInfos[2];
    const i3 = argInfos[3];
    const o0 = argOffsets[0];
    const o1 = argOffsets[1];
    const o2 = argOffsets[2];
    const o3 = argOffsets[3];
    if (retGetter === null) {
      return function(a0, a1, a2, a3) {
        if (arguments.length !== 4) {
          throwFFIArgCountError(4, arguments.length);
        }
        writeNumericArg(view, i0, o0, a0, 0);
        writeNumericArg(view, i1, o1, a1, 1);
        writeNumericArg(view, i2, o2, a2, 2);
        writeNumericArg(view, i3, o3, a3, 3);
        rawFn();
      };
    }
    return function(a0, a1, a2, a3) {
      if (arguments.length !== 4) {
        throwFFIArgCountError(4, arguments.length);
      }
      writeNumericArg(view, i0, o0, a0, 0);
      writeNumericArg(view, i1, o1, a1, 1);
      writeNumericArg(view, i2, o2, a2, 2);
      writeNumericArg(view, i3, o3, a3, 3);
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  if (nargs === 5) {
    const i0 = argInfos[0];
    const i1 = argInfos[1];
    const i2 = argInfos[2];
    const i3 = argInfos[3];
    const i4 = argInfos[4];
    const o0 = argOffsets[0];
    const o1 = argOffsets[1];
    const o2 = argOffsets[2];
    const o3 = argOffsets[3];
    const o4 = argOffsets[4];
    if (retGetter === null) {
      return function(a0, a1, a2, a3, a4) {
        if (arguments.length !== 5) {
          throwFFIArgCountError(5, arguments.length);
        }
        writeNumericArg(view, i0, o0, a0, 0);
        writeNumericArg(view, i1, o1, a1, 1);
        writeNumericArg(view, i2, o2, a2, 2);
        writeNumericArg(view, i3, o3, a3, 3);
        writeNumericArg(view, i4, o4, a4, 4);
        rawFn();
      };
    }
    return function(a0, a1, a2, a3, a4) {
      if (arguments.length !== 5) {
        throwFFIArgCountError(5, arguments.length);
      }
      writeNumericArg(view, i0, o0, a0, 0);
      writeNumericArg(view, i1, o1, a1, 1);
      writeNumericArg(view, i2, o2, a2, 2);
      writeNumericArg(view, i3, o3, a3, 3);
      writeNumericArg(view, i4, o4, a4, 4);
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  if (nargs === 6) {
    const i0 = argInfos[0];
    const i1 = argInfos[1];
    const i2 = argInfos[2];
    const i3 = argInfos[3];
    const i4 = argInfos[4];
    const i5 = argInfos[5];
    const o0 = argOffsets[0];
    const o1 = argOffsets[1];
    const o2 = argOffsets[2];
    const o3 = argOffsets[3];
    const o4 = argOffsets[4];
    const o5 = argOffsets[5];
    if (retGetter === null) {
      return function(a0, a1, a2, a3, a4, a5) {
        if (arguments.length !== 6) {
          throwFFIArgCountError(6, arguments.length);
        }
        writeNumericArg(view, i0, o0, a0, 0);
        writeNumericArg(view, i1, o1, a1, 1);
        writeNumericArg(view, i2, o2, a2, 2);
        writeNumericArg(view, i3, o3, a3, 3);
        writeNumericArg(view, i4, o4, a4, 4);
        writeNumericArg(view, i5, o5, a5, 5);
        rawFn();
      };
    }
    return function(a0, a1, a2, a3, a4, a5) {
      if (arguments.length !== 6) {
        throwFFIArgCountError(6, arguments.length);
      }
      writeNumericArg(view, i0, o0, a0, 0);
      writeNumericArg(view, i1, o1, a1, 1);
      writeNumericArg(view, i2, o2, a2, 2);
      writeNumericArg(view, i3, o3, a3, 3);
      writeNumericArg(view, i4, o4, a4, 4);
      writeNumericArg(view, i5, o5, a5, 5);
      rawFn();
      return retGetter(view, 0, true);
    };
  }
  // 7+ args: further specialization is diminishing returns and bloats
  // this builder.
  if (retGetter === null) {
    return function(...args) {
      if (args.length !== nargs) {
        throwFFIArgCountError(nargs, args.length);
      }
      for (let i = 0; i < nargs; i++) {
        writeNumericArg(view, argInfos[i], argOffsets[i], args[i], i);
      }
      rawFn();
    };
  }
  return function(...args) {
    if (args.length !== nargs) {
      throwFFIArgCountError(nargs, args.length);
    }
    for (let i = 0; i < nargs; i++) {
      writeNumericArg(view, argInfos[i], argOffsets[i], args[i], i);
    }
    rawFn();
    return retGetter(view, 0, true);
  };
}

// Accept-set mirrors the native `ParseFunctionSignature` in
// `src/ffi/types.cc`. `ParseFunctionSignature` additionally throws when
// multiple aliases are set at once. The wrapper runs before the native
// call, so those conflicts still surface from the native side regardless
// of which alias we happen to read here.
function sigParams(sig) {
  return sig.parameters ?? sig.arguments ?? [];
}

function sigResult(sig) {
  return sig.result ?? sig.return ?? sig.returns ?? 'void';
}

// The native invoker for SB-eligible symbols is `InvokeFunctionSB`, which
// reads arguments from the shared buffer populated by
// `wrapWithSharedBuffer`. These patches make sure every path that surfaces
// a raw SB-eligible function to user code (`getFunction`, `getFunctions`,
// and the `functions` accessor) returns the wrapper instead.
const rawGetFunction = DynamicLibrary.prototype.getFunction;
const rawGetFunctions = DynamicLibrary.prototype.getFunctions;

DynamicLibrary.prototype.getFunction = function getFunction(name, sig) {
  // Native `DynamicLibrary::GetFunction` validates `sig`, so by the time
  // we have `raw` we know `sig` is a valid object.
  const raw = FunctionPrototypeCall(rawGetFunction, this, name, sig);
  return wrapWithSharedBuffer(raw, sigParams(sig), sigResult(sig));
};

DynamicLibrary.prototype.getFunctions = function getFunctions(definitions) {
  // Native `GetFunctions` switches on `args.Length() > 0`. Zero args
  // returns every cached function, one arg requires an object. Forwarding
  // `undefined` would fail the object check, so drop it when omitted.
  const raw = definitions === undefined ?
    FunctionPrototypeCall(rawGetFunctions, this) :
    FunctionPrototypeCall(rawGetFunctions, this, definitions);
  if (raw === undefined || raw === null) return raw;
  const keys = ObjectKeys(raw);
  const out = { __proto__: null };
  for (let i = 0; i < keys.length; i++) {
    const name = keys[i];
    // No `definitions`: native side returned every cached function, so we
    // wrap using each function's own `kSbParams` / `kSbResult` metadata
    // (same fallback as the `functions` accessor).
    if (definitions === undefined) {
      out[name] = wrapWithSharedBuffer(raw[name]);
    } else {
      const sig = definitions[name];
      out[name] = wrapWithSharedBuffer(raw[name], sigParams(sig), sigResult(sig));
    }
  }
  return out;
};

{
  // The native side installs `functions` as an accessor returning raw
  // functions. Rewrap each access so `lib.functions.foo(...)` goes through
  // the SB wrapper instead of invoking the fast path against an
  // uninitialized buffer.
  const functionsDescriptor =
    ObjectGetOwnPropertyDescriptor(DynamicLibrary.prototype, 'functions');
  /* c8 ignore start */
  if (functionsDescriptor === undefined || !functionsDescriptor.get) {
    // Missing getter means the native and JS sides are out of sync; silently
    // skipping the patch would expose the fast-path-against-uninitialized-buffer
    // footgun this whole block exists to prevent.
    throw new ERR_INTERNAL_ASSERTION(
      'FFI: DynamicLibrary.prototype.functions accessor not found or has no getter');
  }
  /* c8 ignore stop */
  const origGetter = functionsDescriptor.get;
  ObjectDefineProperty(DynamicLibrary.prototype, 'functions', {
    __proto__: null,
    configurable: true,
    enumerable: functionsDescriptor.enumerable,
    get() {
      const raw = FunctionPrototypeCall(origGetter, this);
      if (raw === undefined || raw === null) return raw;
      const wrapped = { __proto__: null };
      const keys = ObjectKeys(raw);
      for (let i = 0; i < keys.length; i++) {
        const name = keys[i];
        wrapped[name] = wrapWithSharedBuffer(raw[name]);
      }
      return wrapped;
    },
  });
}

module.exports = {
  wrapWithSharedBuffer,
};
