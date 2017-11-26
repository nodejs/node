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
 * Tests to verify we're reading in floats correctly
 */
require('../common');
const assert = require('assert');

/*
 * Test (32 bit) float
 */
function test(clazz) {
  const buffer = new clazz(4);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x3f;
  assert.strictEqual(4.600602988224807e-41, buffer.readFloatBE(0));
  assert.strictEqual(1, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0xc0;
  assert.strictEqual(2.6904930515036488e-43, buffer.readFloatBE(0));
  assert.strictEqual(-2, buffer.readFloatLE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xff;
  buffer[2] = 0x7f;
  buffer[3] = 0x7f;
  assert.ok(Number.isNaN(buffer.readFloatBE(0)));
  assert.strictEqual(3.4028234663852886e+38, buffer.readFloatLE(0));

  buffer[0] = 0xab;
  buffer[1] = 0xaa;
  buffer[2] = 0xaa;
  buffer[3] = 0x3e;
  assert.strictEqual(-1.2126478207002966e-12, buffer.readFloatBE(0));
  assert.strictEqual(0.3333333432674408, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  assert.strictEqual(0, buffer.readFloatBE(0));
  assert.strictEqual(0, buffer.readFloatLE(0));
  assert.strictEqual(false, 1 / buffer.readFloatLE(0) < 0);

  buffer[3] = 0x80;
  assert.strictEqual(1.793662034335766e-43, buffer.readFloatBE(0));
  assert.strictEqual(-0, buffer.readFloatLE(0));
  assert.strictEqual(true, 1 / buffer.readFloatLE(0) < 0);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x7f;
  assert.strictEqual(4.609571298396486e-41, buffer.readFloatBE(0));
  assert.strictEqual(Infinity, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0xff;
  assert.strictEqual(4.627507918739843e-41, buffer.readFloatBE(0));
  assert.strictEqual(-Infinity, buffer.readFloatLE(0));
}


test(Buffer);
