'use strict';

const {
  ArrayBufferIsView,
  ObjectDefineProperties,
  TypedArrayPrototypeGetSymbolToStringTag,
} = primordials;

const isTypedArray = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) !== undefined;

const isUint8Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint8Array';

const isUint8ClampedArray = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint8ClampedArray';

const isUint16Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint16Array';

const isUint32Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Uint32Array';

const isInt8Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Int8Array';

const isInt16Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Int16Array';

const isInt32Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Int32Array';

const isFloat32Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Float32Array';

const isFloat64Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'Float64Array';

const isBigInt64Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'BigInt64Array';

const isBigUint64Array = (value) =>
  TypedArrayPrototypeGetSymbolToStringTag(value) === 'BigUint64Array';

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
