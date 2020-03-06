'use strict';

const {
  ArrayBufferIsView,
  TypedArrayPrototypeSymbolToStringTag,
} = primordials;

function isTypedArray(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) !== undefined;
}

function isUint8Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Uint8Array';
}

function isUint8ClampedArray(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Uint8ClampedArray';
}

function isUint16Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Uint16Array';
}

function isUint32Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Uint32Array';
}

function isInt8Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Int8Array';
}

function isInt16Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Int16Array';
}

function isInt32Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Int32Array';
}

function isFloat32Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Float32Array';
}

function isFloat64Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'Float64Array';
}

function isBigInt64Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'BigInt64Array';
}

function isBigUint64Array(value) {
  return TypedArrayPrototypeSymbolToStringTag(value) === 'BigUint64Array';
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
