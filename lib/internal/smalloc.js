'use strict';

const smalloc = process.binding('smalloc');
const kMaxLength = smalloc.kMaxLength;
const kMinType = smalloc.kMinType;
const kMaxType = smalloc.kMaxType;
const util = require('util');

exports.alloc = alloc;
exports.copyOnto = copyOnto;
exports.dispose = dispose;
exports.hasExternalData = hasExternalData;

// don't allow kMaxLength to accidentally be overwritten. it's a lot less
// apparent when a primitive is accidentally changed.
Object.defineProperty(exports, 'kMaxLength', {
  enumerable: true, value: kMaxLength, writable: false
});

Object.defineProperty(exports, 'Types', {
  enumerable: true, value: Object.freeze(smalloc.types), writable: false
});


// usage: obj = alloc(n[, obj][, type]);
function alloc(n, obj, type) {
  n = n >>> 0;

  if (obj === undefined)
    obj = {};

  if (typeof obj === 'number') {
    type = obj >>> 0;
    obj = {};
  } else if (util.isPrimitive(obj)) {
    throw new TypeError('obj must be an Object');
  }

  if (Array.isArray(obj))
    throw new TypeError('obj cannot be an array');
  if (obj instanceof Buffer)
    throw new TypeError('obj cannot be a Buffer');
  if (smalloc.isTypedArray(obj))
    throw new TypeError('obj cannot be a typed array');
  if (smalloc.hasExternalData(obj))
    throw new TypeError('object already has external array data');

  if (type < kMinType || type > kMaxType)
    throw new TypeError('unknown external array type: ' + type);
  if (n > kMaxLength)
    throw new RangeError('Attempt to allocate array larger than maximum ' +
                         'size: 0x' + kMaxLength.toString(16) + ' elements');

  return smalloc.alloc(obj, n, type);
}


function dispose(obj) {
  if (util.isPrimitive(obj))
    throw new TypeError('obj must be an Object');
  if (obj instanceof Buffer)
    throw new TypeError('obj cannot be a Buffer');
  if (smalloc.isTypedArray(obj))
    throw new TypeError('obj cannot be a typed array');
  if (!smalloc.hasExternalData(obj))
    throw new TypeError('obj has no external array data');

  smalloc.dispose(obj);
}


function copyOnto(source, sourceStart, dest, destStart, copyLength) {
  if (util.isPrimitive(source))
    throw new TypeError('source must be an Object');
  if (util.isPrimitive(dest))
    throw new TypeError('dest must be an Object');
  if (!smalloc.hasExternalData(source))
    throw new TypeError('source has no external array data');
  if (!smalloc.hasExternalData(dest))
    throw new TypeError('dest has no external array data');

  return smalloc.copyOnto(source, sourceStart, dest, destStart, copyLength);
}


function hasExternalData(obj) {
  if (util.isPrimitive(obj))
    return false;

  return smalloc.hasExternalData(obj);
}
