'use strict';
const { emitExperimentalWarning } = require('internal/util');
const {
  codes: {
    ERR_ACCESS_DENIED,
    ERR_INVALID_ARG_TYPE,
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
  const functions =
    definitions === undefined ? lib.getFunctions() : lib.getFunctions(definitions);
  return { lib, functions };
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

  if (len === 0) {
    return;
  }

  const targetBuffer = toBuffer(data, len, false);
  const sourceBuffer = Buffer.from(str, encoding);
  const dataLength = Math.min(sourceBuffer.length, len - 1); // Reserve space for null terminator

  sourceBuffer.copy(targetBuffer, 0, 0, dataLength);
  targetBuffer[dataLength] = 0; // Null terminator  
}

function exportBuffer(buffer, data, len) {
  checkFFIPermission();

  if (!Buffer.isBuffer(buffer)) {
    throw new ERR_INVALID_ARG_TYPE('buffer', 'Buffer', buffer);
  }

  validateInteger(len, 'len', 0);

  if (len === 0) {
    return;
  }

  const targetBuffer = toBuffer(data, len, false);
  const dataLength = Math.min(buffer.length, len);
  buffer.copy(targetBuffer, 0, 0, dataLength);
}

module.exports = {
  DynamicLibrary,
  dlopen,
  dlclose,
  dlsym,
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
  toArrayBuffer,
  toBuffer,
};
