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

/*
 * A battery of tests to help us read a series of uints
 */

var common = require('../common');
var ASSERT = require('assert');

/*
 * We need to check the following things:
 *  - We are correctly resolving big endian (doesn't mean anything for 8 bit)
 *  - Correctly resolving little endian (doesn't mean anything for 8 bit)
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test8(clazz) {
  var data = new clazz(4);

  data[0] = 23;
  data[1] = 23;
  data[2] = 23;
  data[3] = 23;
  ASSERT.equal(23, data.readUInt8(0));
  ASSERT.equal(23, data.readUInt8(1));
  ASSERT.equal(23, data.readUInt8(2));
  ASSERT.equal(23, data.readUInt8(3));

  data[0] = 255; /* If it became a signed int, would be -1 */
  ASSERT.equal(255, data.readUInt8(0));
}


/*
 * Test 16 bit unsigned integers. We need to verify the same set as 8 bit, only
 * now some of the issues actually matter:
 *  - We are correctly resolving big endian
 *  - Correctly resolving little endian
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test16(clazz) {
  var data = new clazz(4);

  data[0] = 0;
  data[1] = 0x23;
  data[2] = 0x42;
  data[3] = 0x3f;
  ASSERT.equal(0x23, data.readUInt16BE(0));
  ASSERT.equal(0x2342, data.readUInt16BE(1));
  ASSERT.equal(0x423f, data.readUInt16BE(2));
  ASSERT.equal(0x2300, data.readUInt16LE(0));
  ASSERT.equal(0x4223, data.readUInt16LE(1));
  ASSERT.equal(0x3f42, data.readUInt16LE(2));

  data[0] = 0xfe;
  data[1] = 0xfe;
  ASSERT.equal(0xfefe, data.readUInt16BE(0));
  ASSERT.equal(0xfefe, data.readUInt16LE(0));
}


/*
 * Test 32 bit unsigned integers. We need to verify the same set as 8 bit, only
 * now some of the issues actually matter:
 *  - We are correctly resolving big endian
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test32(clazz) {
  var data = new clazz(8);

  data[0] = 0x32;
  data[1] = 0x65;
  data[2] = 0x42;
  data[3] = 0x56;
  data[4] = 0x23;
  data[5] = 0xff;
  ASSERT.equal(0x32654256, data.readUInt32BE(0));
  ASSERT.equal(0x65425623, data.readUInt32BE(1));
  ASSERT.equal(0x425623ff, data.readUInt32BE(2));
  ASSERT.equal(0x56426532, data.readUInt32LE(0));
  ASSERT.equal(0x23564265, data.readUInt32LE(1));
  ASSERT.equal(0xff235642, data.readUInt32LE(2));
}


test8(Buffer);
test16(Buffer);
test32(Buffer);
