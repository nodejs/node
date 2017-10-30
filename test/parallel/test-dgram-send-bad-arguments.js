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
require('../common');
const assert = require('assert');
const dgram = require('dgram');

const buf = Buffer.from('test');
const host = '127.0.0.1';
const sock = dgram.createSocket('udp4');

assert.throws(function() {
  sock.send();
}, TypeError);  // First argument should be a buffer.

// send(buf, offset, length, port, host)
assert.throws(function() { sock.send(buf, 1, 1, -1, host); }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, 0, host); }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, 65536, host); }, RangeError);

// send(buf, port, host)
assert.throws(function() { sock.send(23, 12345, host); }, TypeError);

// send([buf1, ..], port, host)
assert.throws(function() { sock.send([buf, 23], 12345, host); }, TypeError);
