// Copyright Joyent, Inc. and other Node contributors.

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

/*
 * Test to verify we are using Typed Arrays
 * (http://www.khronos.org/registry/typedarray/specs/latest/) correctly Test to
 * verify Buffer can used in Typed Arrays
 */

var assert = require('assert');
var SlowBuffer = process.binding('buffer').SlowBuffer;
var ArrayBuffer = process.binding('typed_array').ArrayBuffer;
var Int32Array = process.binding('typed_array').Int32Array;
var Int16Array = process.binding('typed_array').Int16Array;
var Uint8Array = process.binding('typed_array').Uint8Array;

function test(clazz) {
  var size = clazz.length;
  var b = clazz;

  // create a view v1 referring to b, of type Int32, starting at
  // the default byte index (0) and extending until the end of the buffer
  var v1 = new Int32Array(b);
  assert(4, v1.BYTES_PER_ELEMENT);

  // create a view v2 referring to b, of type Uint8, starting at
  // byte index 2 and extending until the end of the buffer
  var v2 = new Uint8Array(b, 2);
  assert(1, v1.BYTES_PER_ELEMENT);

  // create a view v3 referring to b, of type Int16, starting at
  // byte index 2 and having a length of 2
  var v3 = new Int16Array(b, 2, 2);
  assert(2, v1.BYTES_PER_ELEMENT);

  // The layout is now
  // var index
  // b = |0|1|2|3|4|5|6|7| bytes (not indexable)
  // v1 = |0 |1 | indices (indexable)
  // v2 = |0|1|2|3|4|5|
  // v3 = |0 |1 |

  // testing values
  v1[0] = 0x1234;
  v1[1] = 0x5678;

  assert(0x1234, v1[0]);
  assert(0x5678, v1[1]);

  assert(0x3, v2[0]);
  assert(0x4, v2[1]);
  assert(0x5, v2[2]);
  assert(0x6, v2[3]);
  assert(0x7, v2[4]);
  assert(0x8, v2[5]);

  assert(0x34, v3[0]);
  assert(0x56, v3[1]);

  // test get/set
  v2.set(1, 0x8);
  v2.set(2, 0xF);
  assert(0x8, v2.get(1));
  assert(0xF, v2.get(2));
  assert(0x38, v3.get(0));
  assert(0xF6, v3.get(1));

  // test subarray
  var v4 = v1.subarray(1);
  assert(Int32Array, typeof v4);
  assert(0xF678, v4[0]);

  // test set with typed array and []
  v2.set([1, 2, 3, 4], 2);
  assert(0x1234, v1[0]);

  var sub = new Int32Array(4);
  sub[0] = 0xabcd;
  v2.set(sub, 1);
  assert(0x3a, v3[0]);
  assert(0xbc, v3[1]);
}

// basic Typed Arrays tests
var size = 8;
var ab = new ArrayBuffer(size);
assert.equal(size, ab.byteLength);
test(ab);

// testing sharing Buffer object
var buffer = new Buffer(size);
test(buffer);
