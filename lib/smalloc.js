// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var smalloc = process.binding('smalloc');
var kMaxLength = smalloc.kMaxLength;
var util = require('util');

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

  if (util.isUndefined(obj))
    obj = {};

  if (util.isNumber(obj)) {
    type = obj >>> 0;
    obj = {};
  } else if (util.isPrimitive(obj)) {
    throw new TypeError('obj must be an Object');
  }

  // 1 == v8::kExternalByteArray, 9 == v8::kExternalPixelArray
  if (type < 1 || type > 9)
    throw new TypeError('unknown external array type: ' + type);
  if (util.isArray(obj))
    throw new TypeError('Arrays are not supported');
  if (n > kMaxLength)
    throw new RangeError('n > kMaxLength');

  return smalloc.alloc(obj, n, type);
}


function dispose(obj) {
  if (util.isPrimitive(obj))
    throw new TypeError('obj must be an Object');
  if (util.isBuffer(obj))
    throw new TypeError('obj cannot be a Buffer');

  smalloc.dispose(obj);
}
