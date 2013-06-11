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
// minimum string size to overflow into external string space
var EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
var ucs2_control = 'a\u0000';
var write_str = 'a';


// first do basic checks
var b = new Buffer(write_str, 'ucs2');
var c = b.toString('binary');
assert.equal(b[0], 0x61);
assert.equal(b[1], 0);
assert.equal(ucs2_control, c);


// grow the strings to proper length
while (write_str.length <= EXTERN_APEX) {
  write_str += write_str;
  ucs2_control += ucs2_control;
}
write_str += write_str.substr(0, EXTERN_APEX - write_str.length);
ucs2_control += ucs2_control.substr(0, EXTERN_APEX * 2 - ucs2_control.length);


// check resultant buffer and output string
var b = new Buffer(write_str, 'ucs2');
// check fist Buffer created from write string
for (var i = 0; i < b.length; i += 2) {
  assert.equal(b[i], 0x61);
  assert.equal(b[i + 1], 0);
}
// create another string to create an external string
var b_bin = b.toString('binary');
var b_ucs = b.toString('ucs2');
// check control against external binary string
assert.equal(ucs2_control, b_bin);
// create buffer copy from external
var c_bin = new Buffer(b_bin, 'binary');
var c_ucs = new Buffer(b_ucs, 'ucs2');
// make sure they're the same length
assert.equal(c_bin.length, c_ucs.length);
// make sure Buffers from externals are the same
for (var i = 0; i < c_bin.length; i++) {
  assert.equal(c_bin[i], c_ucs[i], c_bin[i] + ' == ' + c_ucs[i] +
               ' : index ' + i);
}
// check resultant strings
assert.equal(c_bin.toString('ucs2'), c_ucs.toString('ucs2'));
assert.equal(c_bin.toString('binary'), ucs2_control);
assert.equal(c_ucs.toString('binary'), ucs2_control);
