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
 * Tests to verify slice functionality of ArrayBuffer.
 * (http://www.khronos.org/registry/typedarray/specs/latest/#5)
 */

var common = require('../common');
var assert = require('assert');

var buffer = new ArrayBuffer(8);
var sub;
var i;

for (var i = 0; i < 8; i++) {
  buffer[i] = i;
}

// slice a copy of buffer
sub = buffer.slice(2, 6);

// make sure it copied correctly
assert.ok(sub instanceof ArrayBuffer);
assert.equal(sub.byteLength, 4);

for (i = 0; i < 4; i++) {
  assert.equal(sub[i], i+2);
}

// modifications to the new slice shouldn't affect the original
sub[0] = 999;

for (i = 0; i < 8; i++) {
  assert.equal(buffer[i], i);
}

// test optional end param
sub = buffer.slice(5);

assert.ok(sub instanceof ArrayBuffer);
assert.equal(sub.byteLength, 3);
for (i = 0; i < 3; i++) {
  assert.equal(sub[i], i+5);
}

// test clamping
sub = buffer.slice(-3, -1);

assert.ok(sub instanceof ArrayBuffer);
assert.equal(sub.byteLength, 2);
for (i = 0; i < 2; i++) {
  assert.equal(sub[i], i+5);
}

// test invalid clamping range
sub = buffer.slice(-1, -3);

assert.ok(sub instanceof ArrayBuffer);
assert.equal(sub.byteLength, 0);

// test wrong number of arguments
var gotError = false;

try {
  sub = buffer.slice();
} catch (e) {
  gotError = true;
}

assert.ok(gotError);