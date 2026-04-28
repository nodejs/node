'use strict';

const {
  ObjectFreeze,
  ObjectPrototypeToString,
} = primordials;
const { Buffer } = require('buffer');
const { emitExperimentalWarning } = require('internal/util');
const {
  isArrayBufferView,
} = require('internal/util/types');
const {
  codes: {
    ERR_ACCESS_DENIED,
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const permission = require('internal/process/permission');
const {
  validateInteger,
  validateString,
} = require('internal/validators');

emitExperimentalWarning('FFI');

const {
  DynamicLibrary,
  getInt8,
  getUint8,
  getInt16,
  getUint16,
  getInt32,
  getUint32,
  getInt64,
  getUint64,
  getFloat32,
  getFloat64,
  exportBytes,
  getRawPointer,
  setInt8,
  setUint8,
  setInt16,
  setUint16,
  setInt32,
  setUint32,
  setInt64,
  setUint64,
  setFloat32,
  setFloat64,
  toString,
  toBuffer,
  toArrayBuffer,
} = internalBinding('ffi');

require('internal/ffi-shared-buffer');

function checkFFIPermission() {
  if (!permission.isEnabled() || permission.has('ffi')) {
    return;
  }

  throw new ERR_ACCESS_DENIED(
    'Access to this API has been restricted. Use --allow-ffi to manage permissions.',
    'FFI');
}

function dlopen(path, definitions) {
  checkFFIPermission();

  const lib = new DynamicLibrary(path);
  try {
    const functions = definitions === undefined ? ObjectFreeze({ __proto__: null }) : lib.getFunctions(definitions);
    return { lib, functions };
  } catch (error) {
    lib.close();
    throw error;
  }
}

function dlclose(handle) {
  checkFFIPermission();
  handle.close();
}

function dlsym(handle, symbol) {
  checkFFIPermission();
  return handle.getSymbol(symbol);
}

function exportString(str, data, len, encoding = 'utf8') {
  checkFFIPermission();
  validateString(str, 'string');
  validateString(encoding, 'encoding');
  validateInteger(len, 'len', 0);

  let terminatorSize;

  switch (encoding.toLowerCase()) {
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      terminatorSize = 2;
      break;
    default:
      terminatorSize = 1;
      break;
  }

  const sourceBuffer = Buffer.from(str, encoding);
  const requiredLength = sourceBuffer.length + terminatorSize;

  if (len < requiredLength) {
    throw new ERR_OUT_OF_RANGE('len', `>= ${requiredLength}`, len);
  }

  const targetBuffer = toBuffer(data, len, false);
  const dataLength = sourceBuffer.length;

  sourceBuffer.copy(targetBuffer, 0, 0, dataLength);
  targetBuffer.fill(0, dataLength, dataLength + terminatorSize);
}

function exportBuffer(source, data, len) {
  checkFFIPermission();

  if (!Buffer.isBuffer(source)) {
    throw new ERR_INVALID_ARG_TYPE('buffer', 'Buffer', source);
  }

  validateInteger(len, 'len', 0);

  if (len < source.length) {
    throw new ERR_OUT_OF_RANGE('len', `>= ${source.length}`, len);
  }

  exportBytes(source, data, len);
}

function exportArrayBuffer(source, data, len) {
  checkFFIPermission();

  if (ObjectPrototypeToString(source) !== '[object ArrayBuffer]') {
    throw new ERR_INVALID_ARG_TYPE('arrayBuffer', 'ArrayBuffer', source);
  }

  validateInteger(len, 'len', 0);

  if (len < source.byteLength) {
    throw new ERR_OUT_OF_RANGE('len', `>= ${source.byteLength}`, len);
  }

  exportBytes(source, data, len);
}

function exportArrayBufferView(source, data, len) {
  checkFFIPermission();

  if (!isArrayBufferView(source)) {
    throw new ERR_INVALID_ARG_TYPE('arrayBufferView', 'ArrayBufferView', source);
  }

  validateInteger(len, 'len', 0);

  if (len < source.byteLength) {
    throw new ERR_OUT_OF_RANGE('len', `>= ${source.byteLength}`, len);
  }

  exportBytes(source, data, len);
}

const suffix = process.platform === 'win32' ? 'dll' : process.platform === 'darwin' ? 'dylib' : 'so';

const types = ObjectFreeze({
  __proto__: null,
  VOID: 'void',
  POINTER: 'pointer',
  BUFFER: 'buffer',
  ARRAY_BUFFER: 'arraybuffer',
  FUNCTION: 'function',
  BOOL: 'bool',
  CHAR: 'char',
  STRING: 'string',
  FLOAT: 'float',
  DOUBLE: 'double',
  INT_8: 'int8',
  UINT_8: 'uint8',
  INT_16: 'int16',
  UINT_16: 'uint16',
  INT_32: 'int32',
  UINT_32: 'uint32',
  INT_64: 'int64',
  UINT_64: 'uint64',
  FLOAT_32: 'float32',
  FLOAT_64: 'float64',
});


module.exports = {
  DynamicLibrary,
  dlopen,
  dlclose,
  dlsym,
  exportArrayBuffer,
  exportArrayBufferView,
  exportString,
  exportBuffer,
  getInt8,
  getUint8,
  getInt16,
  getUint16,
  getInt32,
  getUint32,
  getInt64,
  getUint64,
  getFloat32,
  getFloat64,
  getRawPointer,
  setInt8,
  setUint8,
  setInt16,
  setUint16,
  setInt32,
  setUint32,
  setInt64,
  setUint64,
  setFloat32,
  setFloat64,
  suffix,
  toString,
  toArrayBuffer,
  toBuffer,
  types,
};
