'use strict';

const {
  ArrayPrototypeIncludes,
  ObjectDefineProperty,
  ReflectApply,
  StringPrototypeIncludes,
  Symbol,
  TypeError,
} = primordials;

const {
  Buffer,
} = require('buffer');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  getRawPointer,
  kFastArguments,
  kFastBufferInvoke,
  kSbSharedBuffer,
} = internalBinding('ffi');

const kFastBuffer = Symbol('kFastBuffer');
const kStringConversionBuffer = Symbol('kStringConversionBuffer');

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

function needsRawPointerConversion(type, rawFn) {
  if (rawFn !== undefined && rawFn[kFastBuffer] === true &&
      (type === 'buffer' || type === 'arraybuffer')) {
    return false;
  }
  return type === 'buffer' || type === 'arraybuffer';
}

function needsPointerLikeConversion(type) {
  return type === 'pointer' || type === 'ptr' || type === 'function';
}

function needsStringPointerConversion(type) {
  return type === 'string' || type === 'str' || needsPointerLikeConversion(type);
}

function needsNullPointerConversion(type) {
  return needsPointerLikeConversion(type) || type === 'string' || type === 'str' ||
         needsRawPointerConversion(type);
}

function needsPointerConversion(type, rawFn) {
  if (rawFn !== undefined && rawFn[kFastBuffer] === true &&
      (type === 'buffer' || type === 'arraybuffer')) {
    return false;
  }
  return needsRawPointerConversion(type, rawFn) ||
         needsNullPointerConversion(type) || needsStringPointerConversion(type);
}

function hasStringPointerArg(type, value) {
  return typeof value === 'string' && needsStringPointerConversion(type);
}

function hasPointerMemoryArg(type, value) {
  return (needsRawPointerConversion(type) || needsStringPointerConversion(type)) &&
         (isArrayBufferView(value) || isAnyArrayBuffer(value));
}

function getStringConversionPointer(owner, value, index) {
  const size = value.length * 3 + 1;
  let buffers = owner[kStringConversionBuffer];
  if (buffers === undefined) {
    buffers = [];
    ObjectDefineProperty(owner, kStringConversionBuffer, {
      __proto__: null,
      configurable: false,
      enumerable: false,
      writable: false,
      value: buffers,
    });
  }
  let entry = buffers[index];
  if (entry !== undefined && entry.string === value) {
    return entry.pointer;
  }
  if (StringPrototypeIncludes(value, '\0')) {
    throwFFIArgError(`Argument ${index} must not contain null bytes`);
  }
  if (entry === undefined || entry.buffer.length < size) {
    const buffer = Buffer.allocUnsafe(size);
    entry = {
      __proto__: null,
      buffer,
      pointer: getRawPointer(buffer),
      string: undefined,
    };
    buffers[index] = entry;
  }

  const buffer = entry.buffer;
  const written = buffer.write(value, 0, size - 1, 'utf8');
  buffer[written] = 0;
  entry.string = value;
  return entry.pointer;
}

function convertPointerArg(type, value, owner, index) {
  if (needsNullPointerConversion(type) &&
      (value === null || value === undefined)) {
    return 0n;
  }
  if (hasStringPointerArg(type, value)) {
    return getStringConversionPointer(owner, value, index);
  }
  if (hasPointerMemoryArg(type, value)) {
    return getRawPointer(value);
  }
  if (needsRawPointerConversion(type)) {
    return getRawPointer(value);
  }
  return value;
}

function getPointerConversionIndexes(argumentsTypes, rawFn) {
  let indexes = null;
  for (let i = 0; i < argumentsTypes.length; i++) {
    if (!needsPointerConversion(argumentsTypes[i], rawFn)) {
      continue;
    }
    if (indexes === null) {
      indexes = [];
    }
    indexes.push(i);
  }
  return indexes;
}

function initializeFastBufferMetadata(rawFn, argumentTypes) {
  if (rawFn === undefined || rawFn === null || argumentTypes === undefined) {
    return;
  }
  if (rawFn[kSbSharedBuffer] !== undefined) {
    return;
  }

  if (rawFn[kFastArguments] !== undefined) {
    for (let i = 0; i < argumentTypes.length; i++) {
      const type = argumentTypes[i];
      if (type === 'buffer' || type === 'arraybuffer') {
        rawFn[kFastBuffer] = true;
        break;
      }
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

  const indexes = getPointerConversionIndexes(argumentTypes, rawFn);
  if (indexes === null) {
    return rawFn;
  }

  const nargs = argumentTypes.length;
  let wrapper;
  if (nargs === 1 && indexes.length === 1 && indexes[0] === 0) {
    const t0 = argumentTypes[0];
    const string0 = needsStringPointerConversion(t0);
    const memory0 = needsRawPointerConversion(t0) || string0;
    const fastBufferInvoke = needsPointerLikeConversion(t0) ?
      rawFn[kFastBufferInvoke] : undefined;
    wrapper = function(a0) {
      if (arguments.length !== 1) {
        throwFFIArgCountError(1, arguments.length);
      }
      let arg = a0;
      if (needsNullPointerConversion(t0) &&
          (arg === null || arg === undefined)) {
        arg = 0n;
      } else if (string0 && typeof arg === 'string') {
        arg = getStringConversionPointer(owner, arg, 0);
      } else if (memory0 && (isArrayBufferView(arg) || isAnyArrayBuffer(arg))) {
        if (fastBufferInvoke !== undefined) {
          return fastBufferInvoke(arg);
        }
        arg = getRawPointer(arg);
      }
      return rawFn(arg);
    };
  } else if (nargs === 2) {
    const c0 = ArrayPrototypeIncludes(indexes, 0);
    const c1 = ArrayPrototypeIncludes(indexes, 1);
    const t0 = argumentTypes[0];
    const t1 = argumentTypes[1];
    wrapper = function(a0, a1) {
      if (arguments.length !== 2) {
        throwFFIArgCountError(2, arguments.length);
      }
      return rawFn(c0 ? convertPointerArg(t0, a0, owner, 0) : a0,
                   c1 ? convertPointerArg(t1, a1, owner, 1) : a1);
    };
  } else if (nargs === 3) {
    const c0 = ArrayPrototypeIncludes(indexes, 0);
    const c1 = ArrayPrototypeIncludes(indexes, 1);
    const c2 = ArrayPrototypeIncludes(indexes, 2);
    const t0 = argumentTypes[0];
    const t1 = argumentTypes[1];
    const t2 = argumentTypes[2];
    wrapper = function(a0, a1, a2) {
      if (arguments.length !== 3) {
        throwFFIArgCountError(3, arguments.length);
      }
      return rawFn(c0 ? convertPointerArg(t0, a0, owner, 0) : a0,
                   c1 ? convertPointerArg(t1, a1, owner, 1) : a1,
                   c2 ? convertPointerArg(t2, a2, owner, 2) : a2);
    };
  } else {
    wrapper = function(...args) {
      if (args.length !== nargs) {
        throwFFIArgCountError(nargs, args.length);
      }
      for (let i = 0; i < indexes.length; i++) {
        const index = indexes[i];
        args[index] = convertPointerArg(
          argumentTypes[index], args[index], owner, index);
      }
      return ReflectApply(rawFn, undefined, args);
    };
  }

  return inheritMetadata(wrapper, rawFn, nargs);
}

module.exports = {
  convertPointerArg,
  hasPointerMemoryArg,
  hasStringPointerArg,
  initializeFastBufferMetadata,
  wrapWithRawPointerConversions,
};
