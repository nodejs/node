'use strict';

const smalloc = process.binding('smalloc');
const kMaxLength = smalloc.kMaxLength;
const util = require('util');

exports.alloc = alloc;
exports.copyOnto = smalloc.copyOnto;
exports.dispose = dispose;
exports.hasExternalData = smalloc.hasExternalData;

// don't allow kMaxLength to accidentally be overwritten. it's a lot less
// apparent when a primitive is accidentally changed.
Object.defineProperty(exports, 'kMaxLength', {
  enumerable: true, value: kMaxLength, writable: false
});

// enumerated values for different external array types
var Types = {};

// Must match enum v8::ExternalArrayType.
Object.defineProperties(Types, {
  'Int8': { enumerable: true, value: 1, writable: false },
  'Uint8': { enumerable: true, value: 2, writable: false },
  'Int16': { enumerable: true, value: 3, writable: false },
  'Uint16': { enumerable: true, value: 4, writable: false },
  'Int32': { enumerable: true, value: 5, writable: false },
  'Uint32': { enumerable: true, value: 6, writable: false },
  'Float': { enumerable: true, value: 7, writable: false },
  'Double': { enumerable: true, value: 8, writable: false },
  'Uint8Clamped': { enumerable: true, value: 9, writable: false }
});

Object.defineProperty(exports, 'Types', {
  enumerable: true, value: Types, writable: false
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

  // 1 == v8::kExternalUint8Array, 9 == v8::kExternalUint8ClampedArray
  if (type < 1 || type > 9)
    throw new TypeError('unknown external array type: ' + type);
  if (Array.isArray(obj))
    throw new TypeError('Arrays are not supported');
  if (n > kMaxLength)
    throw new RangeError('n > kMaxLength');

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
    throw new Error('obj has no external array data');

  smalloc.dispose(obj);
}
