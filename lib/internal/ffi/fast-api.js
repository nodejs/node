'use strict';

const {
  ArrayBufferPrototypeGetDetached,
  ArrayPrototypeIncludes,
  DataViewPrototypeGetBuffer,
  NumberIsInteger,
  ObjectDefineProperty,
  ReflectApply,
  SafeWeakMap,
  StringPrototypeIncludes,
  TypeError,
  TypedArrayPrototypeGetBuffer,
} = primordials;

const {
  Buffer,
} = require('buffer');

const {
  isAnyArrayBuffer,
  isArrayBuffer,
  isArrayBufferView,
  isDataView,
} = require('internal/util/types');

const {
  charIsSigned,
  getRawPointer,
  kFastArguments,
  kFastBufferInvoke,
  uintptrMax,
} = internalBinding('ffi');

const {
  codes: {
    ERR_FFI_LIBRARY_CLOSED,
  },
} = require('internal/errors');

const U64_MAX = 0xFFFFFFFFFFFFFFFFn;
const I64_MAX = 0x7FFFFFFFFFFFFFFFn;
const I64_MIN = -0x8000000000000000n;
const fastLibraryStates = new SafeWeakMap();

// These ranges mirror ToFFIArgument in src/ffi/types.cc. V8's Fast API
// exposes narrow integers as 32-bit values and uses truncating BigInt
// conversions, so the public FFI ranges must be checked before the raw call.
const fastIntegerTypeInfo = {
  __proto__: null,
  i8: { kind: 'number', min: -128, max: 127, label: 'an int8' },
  int8: { kind: 'number', min: -128, max: 127, label: 'an int8' },
  char: charIsSigned ?
    { kind: 'number', min: -128, max: 127, label: 'an int8' } :
    { kind: 'number', min: 0, max: 255, label: 'a uint8' },
  u8: { kind: 'number', min: 0, max: 255, label: 'a uint8' },
  uint8: { kind: 'number', min: 0, max: 255, label: 'a uint8' },
  bool: { kind: 'number', min: 0, max: 255, label: 'a uint8' },
  i16: { kind: 'number', min: -32768, max: 32767, label: 'an int16' },
  int16: { kind: 'number', min: -32768, max: 32767, label: 'an int16' },
  u16: { kind: 'number', min: 0, max: 65535, label: 'a uint16' },
  uint16: { kind: 'number', min: 0, max: 65535, label: 'a uint16' },
  i32: { kind: 'number', min: -2147483648, max: 2147483647, label: 'an int32' },
  int32: { kind: 'number', min: -2147483648, max: 2147483647, label: 'an int32' },
  u32: { kind: 'number', min: 0, max: 4294967295, label: 'a uint32' },
  uint32: { kind: 'number', min: 0, max: 4294967295, label: 'a uint32' },
  i64: { kind: 'bigint', min: I64_MIN, max: I64_MAX, label: 'an int64' },
  int64: { kind: 'bigint', min: I64_MIN, max: I64_MAX, label: 'an int64' },
  u64: { kind: 'bigint', min: 0n, max: U64_MAX, label: 'a uint64' },
  uint64: { kind: 'bigint', min: 0n, max: U64_MAX, label: 'a uint64' },
};

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

function validateFastIntegerArg(type, value, index) {
  const info = fastIntegerTypeInfo[type];
  if (info === undefined) return;
  const validType = info.kind === 'number' ?
    typeof value === 'number' && NumberIsInteger(value) :
    typeof value === 'bigint';
  if (!validType || value < info.min || value > info.max) {
    throwFFIArgError(`Argument ${index} must be ${info.label}`);
  }
}

function needsRawPointerConversion(type) {
  return type === 'buffer' || type === 'arraybuffer';
}

function needsPointerLikeConversion(type) {
  return type === 'pointer' || type === 'ptr' || type === 'function' ||
         type === 'buffer' || type === 'arraybuffer';
}

function needsStringPointerConversion(type) {
  return type === 'string' || type === 'str' || needsPointerLikeConversion(type);
}

function needsNullPointerConversion(type) {
  return needsPointerLikeConversion(type) || type === 'string' || type === 'str' ||
         needsRawPointerConversion(type);
}

function needsPointerConversion(type) {
  return needsRawPointerConversion(type) ||
         needsNullPointerConversion(type) || needsStringPointerConversion(type);
}

function validateFastPointerArg(type, value, index) {
  if (needsPointerConversion(type) && typeof value === 'bigint' &&
      (value < 0n || value > uintptrMax)) {
    throwFFIArgError(
      `Argument ${index} must be a non-negative pointer bigint`);
  }
}

function hasStringPointerArg(type, value) {
  return typeof value === 'string' && needsStringPointerConversion(type);
}

function hasPointerMemoryArg(type, value) {
  return (needsRawPointerConversion(type) || needsStringPointerConversion(type)) &&
         (isArrayBufferView(value) || isAnyArrayBuffer(value));
}

function enterStringConversion(state) {
  if (state.buffers[state.depth] === undefined) {
    state.buffers[state.depth] = [];
  }
  state.depth++;
}

function exitStringConversion(state) {
  state.depth--;
}

function getStringConversionPointer(state, value, index) {
  const size = value.length * 3 + 1;
  const buffers = state.buffers[state.depth - 1];
  let entry = buffers[index];
  if (StringPrototypeIncludes(value, '\0')) {
    throwFFIArgError(`Argument ${index} must not contain null bytes`);
  }
  if (entry === undefined || entry.buffer.length < size) {
    const buffer = Buffer.allocUnsafe(size);
    entry = {
      __proto__: null,
      buffer,
      pointer: getRawPointer(buffer),
    };
    buffers[index] = entry;
  }

  const buffer = entry.buffer;
  const written = buffer.write(value, 0, size - 1, 'utf8');
  buffer[written] = 0;
  return entry.pointer;
}

function getRawPointerArg(value, index) {
  let buffer;
  let isView = false;
  if (isArrayBuffer(value)) {
    buffer = value;
  } else if (isArrayBufferView(value)) {
    isView = true;
    buffer = isDataView(value) ?
      DataViewPrototypeGetBuffer(value) :
      TypedArrayPrototypeGetBuffer(value);
  }

  if (buffer !== undefined && isArrayBuffer(buffer) &&
      ArrayBufferPrototypeGetDetached(buffer)) {
    throwFFIArgError(isView ?
      `Argument ${index} is an ArrayBufferView backed by a detached ArrayBuffer` :
      `Argument ${index} is a detached ArrayBuffer`);
  }

  return getRawPointer(value);
}

function convertPointerArg(type, value, stringState, index) {
  validateFastPointerArg(type, value, index);
  if (needsNullPointerConversion(type) &&
      (value === null || value === undefined)) {
    return 0n;
  }
  if (hasStringPointerArg(type, value)) {
    return getStringConversionPointer(stringState, value, index);
  }
  if (hasPointerMemoryArg(type, value)) {
    return getRawPointerArg(value, index);
  }
  // Pointer-like values (e.g. BigInt addresses) are passed through, matching
  // ToFFIArgument in src/ffi/types.cc and the single-argument fast path.
  return value;
}

function getFastArgumentIndexes(argumentsTypes) {
  let indexes = null;
  for (let i = 0; i < argumentsTypes.length; i++) {
    if (fastIntegerTypeInfo[argumentsTypes[i]] === undefined &&
        !needsPointerConversion(argumentsTypes[i])) {
      continue;
    }
    if (indexes === null) {
      indexes = [];
    }
    indexes.push(i);
  }
  return indexes;
}

function convertFastArg(type, value, stringState, index) {
  validateFastIntegerArg(type, value, index);
  return needsPointerConversion(type) ?
    convertPointerArg(type, value, stringState, index) : value;
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

function markFastLibraryClosed(owner) {
  const state = fastLibraryStates.get(owner);
  if (state !== undefined) {
    state.closed = true;
  }
}

function throwIfFastLibraryClosed(state) {
  if (state.closed) {
    throw new ERR_FFI_LIBRARY_CLOSED();
  }
}

// Concise methods do not have [[Construct]], unlike function expressions.
// Keep wrappers non-constructible to match the native FFI functions.
function wrapWithRawPointerConversions(rawFn, argumentTypes, owner) {
  if (rawFn === undefined || rawFn === null) {
    return rawFn;
  }
  if (argumentTypes === undefined) {
    argumentTypes = rawFn[kFastArguments];
  }
  if (argumentTypes === undefined) {
    return rawFn;
  }

  let state = fastLibraryStates.get(owner);
  if (state === undefined) {
    state = { __proto__: null, closed: false };
    fastLibraryStates.set(owner, state);
  }

  const indexes = getFastArgumentIndexes(argumentTypes) ?? [];

  const stringState = {
    __proto__: null,
    buffers: [],
    depth: 0,
  };

  const nargs = argumentTypes.length;
  let wrapper;
  if (nargs === 1 && indexes.length === 1 && indexes[0] === 0) {
    const t0 = argumentTypes[0];
    const string0 = needsStringPointerConversion(t0);
    const memory0 = needsRawPointerConversion(t0) || string0;
    const fastBufferInvoke = needsPointerLikeConversion(t0) ?
      rawFn[kFastBufferInvoke] : undefined;
    wrapper = { invoke(a0) {
      throwIfFastLibraryClosed(state);
      if (arguments.length !== 1) {
        throwFFIArgCountError(1, arguments.length);
      }
      validateFastIntegerArg(t0, a0, 0);
      validateFastPointerArg(t0, a0, 0);
      let arg = a0;
      if (needsNullPointerConversion(t0) &&
          (arg === null || arg === undefined)) {
        arg = 0n;
      } else if (string0 && typeof arg === 'string') {
        enterStringConversion(stringState);
        try {
          return rawFn(getStringConversionPointer(stringState, arg, 0));
        } finally {
          exitStringConversion(stringState);
        }
      } else if (memory0 && (isArrayBufferView(arg) || isAnyArrayBuffer(arg))) {
        if (fastBufferInvoke !== undefined) {
          return fastBufferInvoke(arg);
        }
        arg = getRawPointerArg(arg, 0);
      }
      return rawFn(arg);
    } }.invoke;
  } else if (nargs === 2) {
    const c0 = ArrayPrototypeIncludes(indexes, 0);
    const c1 = ArrayPrototypeIncludes(indexes, 1);
    const t0 = argumentTypes[0];
    const t1 = argumentTypes[1];
    wrapper = { invoke(a0, a1) {
      throwIfFastLibraryClosed(state);
      if (arguments.length !== 2) {
        throwFFIArgCountError(2, arguments.length);
      }
      const stringCall = (c0 && hasStringPointerArg(t0, a0)) ||
                         (c1 && hasStringPointerArg(t1, a1));
      if (stringCall) enterStringConversion(stringState);
      try {
        return rawFn(c0 ? convertFastArg(t0, a0, stringState, 0) : a0,
                     c1 ? convertFastArg(t1, a1, stringState, 1) : a1);
      } finally {
        if (stringCall) exitStringConversion(stringState);
      }
    } }.invoke;
  } else if (nargs === 3) {
    const c0 = ArrayPrototypeIncludes(indexes, 0);
    const c1 = ArrayPrototypeIncludes(indexes, 1);
    const c2 = ArrayPrototypeIncludes(indexes, 2);
    const t0 = argumentTypes[0];
    const t1 = argumentTypes[1];
    const t2 = argumentTypes[2];
    wrapper = { invoke(a0, a1, a2) {
      throwIfFastLibraryClosed(state);
      if (arguments.length !== 3) {
        throwFFIArgCountError(3, arguments.length);
      }
      const stringCall = (c0 && hasStringPointerArg(t0, a0)) ||
                         (c1 && hasStringPointerArg(t1, a1)) ||
                         (c2 && hasStringPointerArg(t2, a2));
      if (stringCall) enterStringConversion(stringState);
      try {
        return rawFn(c0 ? convertFastArg(t0, a0, stringState, 0) : a0,
                     c1 ? convertFastArg(t1, a1, stringState, 1) : a1,
                     c2 ? convertFastArg(t2, a2, stringState, 2) : a2);
      } finally {
        if (stringCall) exitStringConversion(stringState);
      }
    } }.invoke;
  } else {
    wrapper = { invoke(...args) {
      throwIfFastLibraryClosed(state);
      if (args.length !== nargs) {
        throwFFIArgCountError(nargs, args.length);
      }
      let stringCall = false;
      for (let i = 0; i < indexes.length; i++) {
        const index = indexes[i];
        if (hasStringPointerArg(argumentTypes[index], args[index])) {
          stringCall = true;
          break;
        }
      }
      if (stringCall) enterStringConversion(stringState);
      try {
        for (let i = 0; i < indexes.length; i++) {
          const index = indexes[i];
          args[index] = convertFastArg(
            argumentTypes[index], args[index], stringState, index);
        }
        return ReflectApply(rawFn, undefined, args);
      } finally {
        if (stringCall) exitStringConversion(stringState);
      }
    } }.invoke;
  }

  return inheritMetadata(wrapper, rawFn, nargs);
}

module.exports = {
  convertPointerArg,
  hasPointerMemoryArg,
  hasStringPointerArg,
  markFastLibraryClosed,
  wrapWithRawPointerConversions,
};
