'use strict';
const {
  ArrayBuffer,
  BigInt,
  DataView,
  Symbol,
} = primordials;

const {
  setCallBuffer,
  getBufferPointer: getBufferPointerInternal,
  FfiSignature,
  makeCall,
  getSymbol,
  getLibrary,
  types,
  sizes,
  charIsSigned,
} = internalBinding('ffi');

const {
  isArrayBufferView,
  isAnyArrayBuffer,
} = require('internal/util/types');

const {
  ERR_FFI_LIBRARY_LOAD_FAILED,
  ERR_FFI_SYMBOL_NOT_FOUND,
  ERR_FFI_UNSUPPORTED_TYPE,
  ERR_INVALID_ARG_TYPE,
} = require('internal/errors').codes;

// [ sigPtr | fnPtr | rvalue | avaluesPointers ]
const callBuffer = new ArrayBuffer(4096);
const callBufferPtr = setCallBuffer(callBuffer);
const callBufferDV = new DataView(callBuffer);

const libCache = {};

const POINTER_SIZE = sizes['char*'];
const NULL = Symbol('null');

function isSigned(type) {
  if (type === 'char') return charIsSigned;
  return !type.includes('unsigned') && !type.startsWith('uint');
}

const typesToFfiTypes = {
  'void': 'void',
  'char': 'char',
  'signed char': 'char',
  'unsigned char': 'uchar',
  'short': 'short',
  'short int': 'short',
  'signed short': 'short',
  'signed short int': 'short',
  'unsigned short': 'ushort',
  'unsigned short int': 'ushort',
  'int': 'int',
  'signed': 'int',
  'signed int': 'int',
  'unsigned': 'uint',
  'unsigned int': 'uint',
  'long': 'long',
  'long int': 'long',
  'signed long': 'long',
  'signed long int': 'long',
  'unsigned long': 'ulong',
  'unsigned long int': 'ulong',
  // 'long long': ,
  // 'long long int': ,
  // 'signed long long': ,
  // 'signed long long int': ,
  // 'unsigned long long': ,
  // 'unsigned long long int': ,
  'float': 'float',
  'double': 'double',
  // 'long double': ,
  'pointer': 'pointer',
};

function normalizeTypeToFfiTypes(type) {
  if (type.includes('*')) return typesToFfiTypes.pointer;
  if (/^u?int\d\d?_t/.test(type)) return type.replace(/_t$/, '');
  if (type.includes('long long')) {
    const size = sizes[type];
    const signed = type.includes('unsigned');
    return `${signed ? 'u' : ''}int${size * 8}`;
  }
  if (typesToFfiTypes[type]) return typesToFfiTypes[type];
  throw new ERR_FFI_UNSUPPORTED_TYPE(type);
}

const writers = {
  int8_t(offset, value) {
    callBufferDV.setInt8(offset, value);
    return offset + 1;
  },
  uint8_t(offset, value) {
    callBufferDV.setUint8(offset, value);
    return offset + 1;
  },
  int16_t(offset, value) {
    callBufferDV.setInt16(offset, value, true);
    return offset + 2;
  },
  uint16_t(offset, value) {
    callBufferDV.setUint16(offset, value, true);
    return offset + 2;
  },
  int32_t(offset, value) {
    callBufferDV.setInt32(offset, value, true);
    return offset + 4;
  },
  uint32_t(offset, value) {
    callBufferDV.setUint32(offset, value, true);
    return offset + 4;
  },
  int64_t(offset, value) {
    callBufferDV.setBigInt64(offset, value, true);
    return offset + 8;
  },
  uint64_t(offset, value) {
    callBufferDV.setBigUint64(offset, value, true);
    return offset + 8;
  },
  float(offset, value) {
    callBufferDV.setFloat32(offset, value, true);
    return offset + 4;
  },
  double(offset, value) {
    callBufferDV.setFloat64(offset, value, true);
    return offset + 8;
  },
};
writers.pointer = writers.uint64_t;

function getWriter(type) {
  if (writers[type]) return writers[type];
  if (type.includes('*')) return writers.pointer;
  const signed = isSigned(type);
  const size = sizeof(type);
  return writers[`${signed ? '' : 'u'}int${size * 8}_t`];
}

const readers = {
  int8_t: (offset) => callBufferDV.getInt8(offset),
  uint8_t: (offset) => callBufferDV.getUint8(offset),
  int16_t: (offset) => callBufferDV.getUint16(offset, true),
  uint16_t: (offset) => callBufferDV.getUint16(offset, true),
  int32_t: (offset) => callBufferDV.getUint32(offset, true),
  uint32_t: (offset) => callBufferDV.getUint32(offset, true),
  int64_t: (offset) => callBufferDV.getBigInt64(offset, true),
  uint64_t: (offset) => callBufferDV.getBigUint64(offset, true),
  float: (offset) => callBufferDV.getFloat32(offset, true),
  double: (offset) => callBufferDV.getFloat64(offset, true),
  void: () => undefined,
};
readers.pointer = readers.uint64_t;

function getReader(type) {
  if (readers[type]) return readers[type];
  if (type.includes('*')) return readers.pointer;
  const signed = isSigned(type);
  const size = sizeof(type);
  return readers[`${signed ? '' : 'u'}int${size * 8}_t`];
}

function getNativeFunction(lib, funcName, ret, args) {
  if (lib !== null && typeof lib !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('library', ['string', 'null'], lib);
  }
  if (lib === null) {
    lib = NULL;
  }
  const getReturnVal = getReader(ret);
  const argWriters = args.map(getWriter);
  ret = normalizeTypeToFfiTypes(ret);
  args = args.map(normalizeTypeToFfiTypes);

  let libPtr = libCache[lib];
  if (!libCache[lib]) {
    libPtr = getLibrary(lib === NULL ? null : lib);
    if (!libPtr) {
      throw new ERR_FFI_LIBRARY_LOAD_FAILED(lib === null ? 'null' : lib);
    }
    libCache[lib] = libPtr;
  }

  const funcPtr = getSymbol(libPtr, funcName);
  if (funcPtr === 0n) {
    throw new ERR_FFI_SYMBOL_NOT_FOUND(funcName, lib === null ? 'null' : lib);
  }

  const sig = new FfiSignature(funcPtr, types[ret], args.map((n) => types[n]));

  return function(...callArgs) {
    let offset = 0;
    offset = writers.pointer(offset, sig.pointer);
    offset += POINTER_SIZE; // for the return value
    let argsOffset = offset + (args.length * POINTER_SIZE);
    for (let i = 0; i < args.length; i++) {
      offset = writers.pointer(offset, callBufferPtr + BigInt(argsOffset));
      argsOffset = argWriters[i](argsOffset, callArgs[i]);
    }
    makeCall();
    return getReturnVal(8);
  };
}

function getBufferPointer(buf) {
  let offset = 0n;
  if (isArrayBufferView(buf)) {
    buf = buf.buffer;
    offset = BigInt(buf.byteOffset || 0);
  }
  if (!isAnyArrayBuffer(buf)) {
    throw new ERR_INVALID_ARG_TYPE('buffer', [
      'Buffer',
      'TypedArray',
      'DataView',
      'ArrayBuffer',
      'SharedArrayBuffer',
    ], buf);
  }
  const ptr = getBufferPointerInternal(buf);
  return ptr + offset;
}

function sizeof(typ) {
  if (sizes[typ]) return sizes[typ];
  if (typ.includes('*')) return sizes['char*'];
}

module.exports = {
  getBufferPointer,
  getNativeFunction,
  sizeof,
};
