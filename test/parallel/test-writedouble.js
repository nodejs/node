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

'use strict';
/*
 * Tests to verify we're writing doubles correctly
 */
require('../common');
const assert = require('assert');

function test(clazz) {
  const buffer = new clazz(16);

  buffer.writeDoubleBE(2.225073858507201e-308, 0);
  buffer.writeDoubleLE(2.225073858507201e-308, 8);
  assert.strictEqual(0x00, buffer[0]);
  assert.strictEqual(0x0f, buffer[1]);
  assert.strictEqual(0xff, buffer[2]);
  assert.strictEqual(0xff, buffer[3]);
  assert.strictEqual(0xff, buffer[4]);
  assert.strictEqual(0xff, buffer[5]);
  assert.strictEqual(0xff, buffer[6]);
  assert.strictEqual(0xff, buffer[7]);
  assert.strictEqual(0xff, buffer[8]);
  assert.strictEqual(0xff, buffer[9]);
  assert.strictEqual(0xff, buffer[10]);
  assert.strictEqual(0xff, buffer[11]);
  assert.strictEqual(0xff, buffer[12]);
  assert.strictEqual(0xff, buffer[13]);
  assert.strictEqual(0x0f, buffer[14]);
  assert.strictEqual(0x00, buffer[15]);

  buffer.writeDoubleBE(1.0000000000000004, 0);
  buffer.writeDoubleLE(1.0000000000000004, 8);
  assert.strictEqual(0x3f, buffer[0]);
  assert.strictEqual(0xf0, buffer[1]);
  assert.strictEqual(0x00, buffer[2]);
  assert.strictEqual(0x00, buffer[3]);
  assert.strictEqual(0x00, buffer[4]);
  assert.strictEqual(0x00, buffer[5]);
  assert.strictEqual(0x00, buffer[6]);
  assert.strictEqual(0x02, buffer[7]);
  assert.strictEqual(0x02, buffer[8]);
  assert.strictEqual(0x00, buffer[9]);
  assert.strictEqual(0x00, buffer[10]);
  assert.strictEqual(0x00, buffer[11]);
  assert.strictEqual(0x00, buffer[12]);
  assert.strictEqual(0x00, buffer[13]);
  assert.strictEqual(0xf0, buffer[14]);
  assert.strictEqual(0x3f, buffer[15]);

  buffer.writeDoubleBE(-2, 0);
  buffer.writeDoubleLE(-2, 8);
  assert.strictEqual(0xc0, buffer[0]);
  assert.strictEqual(0x00, buffer[1]);
  assert.strictEqual(0x00, buffer[2]);
  assert.strictEqual(0x00, buffer[3]);
  assert.strictEqual(0x00, buffer[4]);
  assert.strictEqual(0x00, buffer[5]);
  assert.strictEqual(0x00, buffer[6]);
  assert.strictEqual(0x00, buffer[7]);
  assert.strictEqual(0x00, buffer[8]);
  assert.strictEqual(0x00, buffer[9]);
  assert.strictEqual(0x00, buffer[10]);
  assert.strictEqual(0x00, buffer[11]);
  assert.strictEqual(0x00, buffer[12]);
  assert.strictEqual(0x00, buffer[13]);
  assert.strictEqual(0x00, buffer[14]);
  assert.strictEqual(0xc0, buffer[15]);

  buffer.writeDoubleBE(1.7976931348623157e+308, 0);
  buffer.writeDoubleLE(1.7976931348623157e+308, 8);
  assert.strictEqual(0x7f, buffer[0]);
  assert.strictEqual(0xef, buffer[1]);
  assert.strictEqual(0xff, buffer[2]);
  assert.strictEqual(0xff, buffer[3]);
  assert.strictEqual(0xff, buffer[4]);
  assert.strictEqual(0xff, buffer[5]);
  assert.strictEqual(0xff, buffer[6]);
  assert.strictEqual(0xff, buffer[7]);
  assert.strictEqual(0xff, buffer[8]);
  assert.strictEqual(0xff, buffer[9]);
  assert.strictEqual(0xff, buffer[10]);
  assert.strictEqual(0xff, buffer[11]);
  assert.strictEqual(0xff, buffer[12]);
  assert.strictEqual(0xff, buffer[13]);
  assert.strictEqual(0xef, buffer[14]);
  assert.strictEqual(0x7f, buffer[15]);

  buffer.writeDoubleBE(0 * -1, 0);
  buffer.writeDoubleLE(0 * -1, 8);
  assert.strictEqual(0x80, buffer[0]);
  assert.strictEqual(0x00, buffer[1]);
  assert.strictEqual(0x00, buffer[2]);
  assert.strictEqual(0x00, buffer[3]);
  assert.strictEqual(0x00, buffer[4]);
  assert.strictEqual(0x00, buffer[5]);
  assert.strictEqual(0x00, buffer[6]);
  assert.strictEqual(0x00, buffer[7]);
  assert.strictEqual(0x00, buffer[8]);
  assert.strictEqual(0x00, buffer[9]);
  assert.strictEqual(0x00, buffer[10]);
  assert.strictEqual(0x00, buffer[11]);
  assert.strictEqual(0x00, buffer[12]);
  assert.strictEqual(0x00, buffer[13]);
  assert.strictEqual(0x00, buffer[14]);
  assert.strictEqual(0x80, buffer[15]);

  buffer.writeDoubleBE(Infinity, 0);
  buffer.writeDoubleLE(Infinity, 8);
  assert.strictEqual(0x7F, buffer[0]);
  assert.strictEqual(0xF0, buffer[1]);
  assert.strictEqual(0x00, buffer[2]);
  assert.strictEqual(0x00, buffer[3]);
  assert.strictEqual(0x00, buffer[4]);
  assert.strictEqual(0x00, buffer[5]);
  assert.strictEqual(0x00, buffer[6]);
  assert.strictEqual(0x00, buffer[7]);
  assert.strictEqual(0x00, buffer[8]);
  assert.strictEqual(0x00, buffer[9]);
  assert.strictEqual(0x00, buffer[10]);
  assert.strictEqual(0x00, buffer[11]);
  assert.strictEqual(0x00, buffer[12]);
  assert.strictEqual(0x00, buffer[13]);
  assert.strictEqual(0xF0, buffer[14]);
  assert.strictEqual(0x7F, buffer[15]);
  assert.strictEqual(Infinity, buffer.readDoubleBE(0));
  assert.strictEqual(Infinity, buffer.readDoubleLE(8));

  buffer.writeDoubleBE(-Infinity, 0);
  buffer.writeDoubleLE(-Infinity, 8);
  assert.strictEqual(0xFF, buffer[0]);
  assert.strictEqual(0xF0, buffer[1]);
  assert.strictEqual(0x00, buffer[2]);
  assert.strictEqual(0x00, buffer[3]);
  assert.strictEqual(0x00, buffer[4]);
  assert.strictEqual(0x00, buffer[5]);
  assert.strictEqual(0x00, buffer[6]);
  assert.strictEqual(0x00, buffer[7]);
  assert.strictEqual(0x00, buffer[8]);
  assert.strictEqual(0x00, buffer[9]);
  assert.strictEqual(0x00, buffer[10]);
  assert.strictEqual(0x00, buffer[11]);
  assert.strictEqual(0x00, buffer[12]);
  assert.strictEqual(0x00, buffer[13]);
  assert.strictEqual(0xF0, buffer[14]);
  assert.strictEqual(0xFF, buffer[15]);
  assert.strictEqual(-Infinity, buffer.readDoubleBE(0));
  assert.strictEqual(-Infinity, buffer.readDoubleLE(8));

  buffer.writeDoubleBE(NaN, 0);
  buffer.writeDoubleLE(NaN, 8);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert(0x7F === buffer[0] || 0xFF === buffer[0]);
  // mips processors use a slightly different NaN
  assert(0xF8 === buffer[1] || 0xF7 === buffer[1]);
  assert(0x00 === buffer[2] || 0xFF === buffer[2]);
  assert(0x00 === buffer[3] || 0xFF === buffer[3]);
  assert(0x00 === buffer[4] || 0xFF === buffer[4]);
  assert(0x00 === buffer[5] || 0xFF === buffer[5]);
  assert(0x00 === buffer[6] || 0xFF === buffer[6]);
  assert(0x00 === buffer[7] || 0xFF === buffer[7]);
  assert(0x00 === buffer[8] || 0xFF === buffer[8]);
  assert(0x00 === buffer[9] || 0xFF === buffer[9]);
  assert(0x00 === buffer[10] || 0xFF === buffer[10]);
  assert(0x00 === buffer[11] || 0xFF === buffer[11]);
  assert(0x00 === buffer[12] || 0xFF === buffer[12]);
  assert(0x00 === buffer[13] || 0xFF === buffer[13]);
  assert(0xF8 === buffer[14] || 0xF7 === buffer[14]);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert(0x7F === buffer[15] || 0xFF === buffer[15]);
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.ok(isNaN(buffer.readDoubleLE(8)));
}


test(Buffer);
