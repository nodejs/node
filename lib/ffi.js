'use strict';

const {
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectGetOwnPropertyDescriptor,
  ObjectKeys,
  ObjectPrototypeToString,
  SymbolDispose,
} = primordials;
const { Buffer } = require('buffer');
const { emitExperimentalWarning } = require('internal/util');
const {
  isArrayBufferView,
} = require('internal/util/types');
const {
  codes: {
    ERR_ACCESS_DENIED,
    ERR_INTERNAL_ASSERTION,
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
  getCurrentEventLoop,
  exportBytes,
  getRawPointer,
  kFastArguments,
  kSbArguments,
  kSbReturn,
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

const {
  wrapWithSharedBuffer,
} = require('internal/ffi-shared-buffer');

const {
  initializeFastBufferMetadata,
  wrapWithRawPointerConversions,
} = require('internal/ffi/fast-api');

function makeSignature(argumentTypes, returnType) {
  return {
    __proto__: null,
    arguments: argumentTypes,
    return: returnType,
  };
}

function wrapFFIFunction(rawFn, argumentTypes, returnType, owner) {
  if (argumentTypes === undefined && rawFn !== undefined && rawFn !== null) {
    const sbArguments = rawFn[kSbArguments];
    argumentTypes = sbArguments ?? rawFn[kFastArguments];
    if (sbArguments !== undefined) {
      returnType = rawFn[kSbReturn];
    }
  }
  initializeFastBufferMetadata(rawFn, argumentTypes);
  const wrapped = wrapWithSharedBuffer(
    rawFn,
    argumentTypes === undefined ? undefined : makeSignature(argumentTypes, returnType));
  if (wrapped !== rawFn) {
    return wrapped;
  }
  return wrapWithRawPointerConversions(rawFn, argumentTypes, owner);
}

const rawGetFunction = DynamicLibrary.prototype.getFunction;
const rawGetFunctions = DynamicLibrary.prototype.getFunctions;

DynamicLibrary.prototype.getFunction = function getFunction(name, signature) {
  const raw = FunctionPrototypeCall(rawGetFunction, this, name, signature);
  return wrapFFIFunction(raw, signature.arguments, signature.return, this);
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
      out[name] = wrapFFIFunction(raw[name], undefined, undefined, this);
    } else {
      const signature = definitions[name];
      out[name] = wrapFFIFunction(
        raw[name], signature.arguments, signature.return, this);
    }
  }
  return out;
};

{
  const functionsDescriptor =
    ObjectGetOwnPropertyDescriptor(DynamicLibrary.prototype, 'functions');
  if (functionsDescriptor === undefined || functionsDescriptor.get === undefined) {
    throw new ERR_INTERNAL_ASSERTION(
      'FFI: DynamicLibrary.prototype.functions accessor not found or has no getter');
  }
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
        wrapped[name] = wrapFFIFunction(raw[name], undefined, undefined, this);
      }
      return wrapped;
    },
  });
}

function checkFFIPermission() {
  if (!permission.isEnabled()) {
    return;
  }

  if (permission.has('ffi') || permission.isAuditMode()) {
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
    return {
      lib,
      functions,
      [SymbolDispose]() { lib.close(); },
    };
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
  getCurrentEventLoop,
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
