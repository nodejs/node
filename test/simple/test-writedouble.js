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
 * Tests to verify we're writing doubles correctly
 */
var SlowBuffer = process.binding('buffer').SlowBuffer;
var ASSERT = require('assert');

function test(clazz) {
  var buffer = new clazz(16);

  buffer.writeDoubleBE(2.225073858507201e-308, 0);
  buffer.writeDoubleLE(2.225073858507201e-308, 8);
  ASSERT.equal(0x00, buffer[0]);
  ASSERT.equal(0x0f, buffer[1]);
  ASSERT.equal(0xff, buffer[2]);
  ASSERT.equal(0xff, buffer[3]);
  ASSERT.equal(0xff, buffer[4]);
  ASSERT.equal(0xff, buffer[5]);
  ASSERT.equal(0xff, buffer[6]);
  ASSERT.equal(0xff, buffer[7]);
  ASSERT.equal(0xff, buffer[8]);
  ASSERT.equal(0xff, buffer[9]);
  ASSERT.equal(0xff, buffer[10]);
  ASSERT.equal(0xff, buffer[11]);
  ASSERT.equal(0xff, buffer[12]);
  ASSERT.equal(0xff, buffer[13]);
  ASSERT.equal(0x0f, buffer[14]);
  ASSERT.equal(0x00, buffer[15]);

  buffer.writeDoubleBE(1.0000000000000004, 0);
  buffer.writeDoubleLE(1.0000000000000004, 8);
  ASSERT.equal(0x3f, buffer[0]);
  ASSERT.equal(0xf0, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x02, buffer[7]);
  ASSERT.equal(0x02, buffer[8]);
  ASSERT.equal(0x00, buffer[9]);
  ASSERT.equal(0x00, buffer[10]);
  ASSERT.equal(0x00, buffer[11]);
  ASSERT.equal(0x00, buffer[12]);
  ASSERT.equal(0x00, buffer[13]);
  ASSERT.equal(0xf0, buffer[14]);
  ASSERT.equal(0x3f, buffer[15]);

  buffer.writeDoubleBE(-2, 0);
  buffer.writeDoubleLE(-2, 8);
  ASSERT.equal(0xc0, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x00, buffer[7]);
  ASSERT.equal(0x00, buffer[8]);
  ASSERT.equal(0x00, buffer[9]);
  ASSERT.equal(0x00, buffer[10]);
  ASSERT.equal(0x00, buffer[11]);
  ASSERT.equal(0x00, buffer[12]);
  ASSERT.equal(0x00, buffer[13]);
  ASSERT.equal(0x00, buffer[14]);
  ASSERT.equal(0xc0, buffer[15]);

  buffer.writeDoubleBE(1.7976931348623157e+308, 0);
  buffer.writeDoubleLE(1.7976931348623157e+308, 8);
  ASSERT.equal(0x7f, buffer[0]);
  ASSERT.equal(0xef, buffer[1]);
  ASSERT.equal(0xff, buffer[2]);
  ASSERT.equal(0xff, buffer[3]);
  ASSERT.equal(0xff, buffer[4]);
  ASSERT.equal(0xff, buffer[5]);
  ASSERT.equal(0xff, buffer[6]);
  ASSERT.equal(0xff, buffer[7]);
  ASSERT.equal(0xff, buffer[8]);
  ASSERT.equal(0xff, buffer[9]);
  ASSERT.equal(0xff, buffer[10]);
  ASSERT.equal(0xff, buffer[11]);
  ASSERT.equal(0xff, buffer[12]);
  ASSERT.equal(0xff, buffer[13]);
  ASSERT.equal(0xef, buffer[14]);
  ASSERT.equal(0x7f, buffer[15]);

  buffer.writeDoubleBE(0 * -1, 0);
  buffer.writeDoubleLE(0 * -1, 8);
  ASSERT.equal(0x80, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x00, buffer[7]);
  ASSERT.equal(0x00, buffer[8]);
  ASSERT.equal(0x00, buffer[9]);
  ASSERT.equal(0x00, buffer[10]);
  ASSERT.equal(0x00, buffer[11]);
  ASSERT.equal(0x00, buffer[12]);
  ASSERT.equal(0x00, buffer[13]);
  ASSERT.equal(0x00, buffer[14]);
  ASSERT.equal(0x80, buffer[15]);
}


test(Buffer);
test(SlowBuffer);
