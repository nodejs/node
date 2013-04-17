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

var common = require('../common');
var assert = require('assert');

var smalloc = process.binding('smalloc');
var alloc = smalloc.alloc;
var copyOnto = smalloc.copyOnto;
var sliceOnto = smalloc.sliceOnto;


// verify initial allocation

var b = alloc({}, 5);
assert.ok(typeof b === 'object');
for (var i = 0; i < 5; i++)
  assert.ok(b[i] !== undefined);


var b = {};
var c = alloc(b, 5);
assert.equal(b, c);
assert.deepEqual(b, c);


var b = alloc({}, 5);
var c = {};
c._data = sliceOnto(b, c, 0, 5);
assert.ok(typeof c._data === 'object');
assert.equal(b, c._data);
assert.deepEqual(b, c._data);


// verify writes

var b = alloc({}, 5);
for (var i = 0; i < 5; i++)
  b[i] = i;
for (var i = 0; i < 5; i++)
  assert.equal(b[i], i);


var b = alloc({}, 6);
var c0 = {};
var c1 = {};
c0._data = sliceOnto(b, c0, 0, 3);
c1._data = sliceOnto(b, c1, 3, 6);
for (var i = 0; i < 3; i++) {
  c0[i] = i;
  c1[i] = i + 3;
}
for (var i = 0; i < 3; i++)
  assert.equal(b[i], i);
for (var i = 3; i < 6; i++)
  assert.equal(b[i], i);


var a = alloc({}, 6);
var b = alloc({}, 6);
var c = alloc({}, 12);
for (var i = 0; i < 6; i++) {
  a[i] = i;
  b[i] = i * 2;
}
copyOnto(a, 0, c, 0, 6);
copyOnto(b, 0, c, 6, 6);
for (var i = 0; i < 6; i++) {
  assert.equal(c[i], i);
  assert.equal(c[i + 6], i * 2);
}
