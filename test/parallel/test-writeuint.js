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

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  ASSERT.equal(23, data[0]);
  ASSERT.equal(23, data[1]);
  ASSERT.equal(23, data[2]);
  ASSERT.equal(23, data[3]);

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  ASSERT.equal(23, data[0]);
  ASSERT.equal(23, data[1]);
  ASSERT.equal(23, data[2]);
  ASSERT.equal(23, data[3]);

  data.writeUInt8(255, 0);
  ASSERT.equal(255, data[0]);

  data.writeUInt8(255, 0);
  ASSERT.equal(255, data[0]);
}


function test16(clazz) {
  var value = 0x2343;
  var data = new clazz(4);

  data.writeUInt16BE(value, 0);
  ASSERT.equal(0x23, data[0]);
  ASSERT.equal(0x43, data[1]);

  data.writeUInt16BE(value, 1);
  ASSERT.equal(0x23, data[1]);
  ASSERT.equal(0x43, data[2]);

  data.writeUInt16BE(value, 2);
  ASSERT.equal(0x23, data[2]);
  ASSERT.equal(0x43, data[3]);

  data.writeUInt16LE(value, 0);
  ASSERT.equal(0x23, data[1]);
  ASSERT.equal(0x43, data[0]);

  data.writeUInt16LE(value, 1);
  ASSERT.equal(0x23, data[2]);
  ASSERT.equal(0x43, data[1]);

  data.writeUInt16LE(value, 2);
  ASSERT.equal(0x23, data[3]);
  ASSERT.equal(0x43, data[2]);

  value = 0xff80;
  data.writeUInt16LE(value, 0);
  ASSERT.equal(0xff, data[1]);
  ASSERT.equal(0x80, data[0]);

  data.writeUInt16BE(value, 0);
  ASSERT.equal(0xff, data[0]);
  ASSERT.equal(0x80, data[1]);
}


function test32(clazz) {
  var data = new clazz(6);
  var value = 0xe7f90a6d;

  data.writeUInt32BE(value, 0);
  ASSERT.equal(0xe7, data[0]);
  ASSERT.equal(0xf9, data[1]);
  ASSERT.equal(0x0a, data[2]);
  ASSERT.equal(0x6d, data[3]);

  data.writeUInt32BE(value, 1);
  ASSERT.equal(0xe7, data[1]);
  ASSERT.equal(0xf9, data[2]);
  ASSERT.equal(0x0a, data[3]);
  ASSERT.equal(0x6d, data[4]);

  data.writeUInt32BE(value, 2);
  ASSERT.equal(0xe7, data[2]);
  ASSERT.equal(0xf9, data[3]);
  ASSERT.equal(0x0a, data[4]);
  ASSERT.equal(0x6d, data[5]);

  data.writeUInt32LE(value, 0);
  ASSERT.equal(0xe7, data[3]);
  ASSERT.equal(0xf9, data[2]);
  ASSERT.equal(0x0a, data[1]);
  ASSERT.equal(0x6d, data[0]);

  data.writeUInt32LE(value, 1);
  ASSERT.equal(0xe7, data[4]);
  ASSERT.equal(0xf9, data[3]);
  ASSERT.equal(0x0a, data[2]);
  ASSERT.equal(0x6d, data[1]);

  data.writeUInt32LE(value, 2);
  ASSERT.equal(0xe7, data[5]);
  ASSERT.equal(0xf9, data[4]);
  ASSERT.equal(0x0a, data[3]);
  ASSERT.equal(0x6d, data[2]);
}


test8(Buffer);
test16(Buffer);
test32(Buffer);
