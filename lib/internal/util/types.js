'use strict';

const { uncurryThis } = require('primordials');

const TypedArrayPrototype = Object.getPrototypeOf(Uint8Array.prototype);

const TypedArrayProto_toStringTag =
    uncurryThis(
      Object.getOwnPropertyDescriptor(TypedArrayPrototype,
                                      Symbol.toStringTag).get);

// Cached to make sure no userland code can tamper with it.
const isArrayBufferView = ArrayBuffer.isView;

function isTypedArray(value) {
  return TypedArrayProto_toStringTag(value) !== undefined;
}

function isUint8Array(value) {
  return TypedArrayProto_toStringTag(value) === 'Uint8Array';
}

module.exports = {
  isArrayBufferView,
  isTypedArray,
  isUint8Array
};
