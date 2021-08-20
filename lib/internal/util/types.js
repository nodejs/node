'use strict';

const {
  ArrayBufferIsView,
  ObjectDefineProperties,
  TypedArrayPrototypeGetSymbolToStringTag,
} = primordials;

function isTypedArray(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) !== undefined;
}

function isUint8Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint8Array';
}

function isUint8ClampedArray(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint8ClampedArray';
}

function isUint16Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint16Array';
}

function isUint32Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint32Array';
}

function isInt8Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Int8Array';
}

function isInt16Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Int16Array';
}

function isInt32Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Int32Array';
}

function isFloat32Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Float32Array';
}

function isFloat64Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'Float64Array';
}

function isBigInt64Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'BigInt64Array';
}

function isBigUint64Array(value) {
  return TypedArrayPrototypeGetSymbolToStringTag(value) === 'BigUint64Array';
}

module.exports = {
  ...internalBinding('types'),
  isArrayBufferView: ArrayBufferIsView,
  isTypedArray,
  isUint8Array,
  isUint8ClampedArray,
  isUint16Array,
  isUint32Array,
  isInt8Array,
  isInt16Array,
  isInt32Array,
  isFloat32Array,
  isFloat64Array,
  isBigInt64Array,
  isBigUint64Array
};

let isCryptoKey;
let isKeyObject;

ObjectDefineProperties(module.exports, {
  isKeyObject: {
    configurable: false,
    enumerable: true,
    value(obj) {
      if (!process.versions.openssl) {
        return false;
      }

      if (!isKeyObject) {
        ({ isKeyObject } = require('internal/crypto/keys'));
      }

      return isKeyObject(obj);
    }
  },
  isCryptoKey: {
    configurable: false,
    enumerable: true,
    value(obj) {
      if (!process.versions.openssl) {
        return false;
      }

      if (!isCryptoKey) {
        ({ isCryptoKey } = require('internal/crypto/keys'));
      }

      return isCryptoKey(obj);
    }
  }
});
