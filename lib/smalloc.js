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

// don't allow kMaxLength to accidentally be overwritten. it's a lot less
// apparent when a primitive is accidentally changed.
Object.defineProperty(exports, 'kMaxLength', {
  enumerable: true,
  value: kMaxLength,
  writable: false
});


function alloc(n, obj) {
  n = n >>> 0;

  if (util.isUndefined(obj))
    obj = {};
  else if (util.isPrimitive(obj))
    throw new TypeError('obj must be an Object');
  if (n > kMaxLength)
    throw new RangeError('n > kMaxLength');
  if (util.isArray(obj))
    throw new TypeError('Arrays are not supported');

  return smalloc.alloc(obj, n);
}


function dispose(obj) {
  if (util.isPrimitive(obj))
    throw new TypeError('obj must be an Object');
  if (util.isBuffer(obj))
    throw new TypeError('obj cannot be a Buffer');

  smalloc.dispose(obj);
}
