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

// This is the same as test/simple/test-crypto, but from before the shift
// to use buffers by default.


var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

var EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
var ucs2_control = 'a\u0000';

// grow the strings to proper length
while (ucs2_control.length <= EXTERN_APEX) {
  ucs2_control += ucs2_control;
}


// check resultant buffer and output string
var b = new Buffer(ucs2_control + ucs2_control, 'ucs2');

//
// Test updating from birant data
//
(function() {
  var datum1 = b.slice(700000);
  var hash1_converted = crypto.createHash('sha1')
    .update(datum1.toString('base64'), 'base64')
    .digest('hex');
  var hash1_direct = crypto.createHash('sha1').update(datum1).digest('hex');
  assert.equal(hash1_direct, hash1_converted, 'should hash the same.');

  var datum2 = b;
  var hash2_converted = crypto.createHash('sha1')
    .update(datum2.toString('base64'), 'base64')
    .digest('hex');
  var hash2_direct = crypto.createHash('sha1').update(datum2).digest('hex');
  assert.equal(hash2_direct, hash2_converted, 'should hash the same.');
})();
