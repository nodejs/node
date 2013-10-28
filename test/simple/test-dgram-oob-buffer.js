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

// Some operating systems report errors when an UDP message is sent to an
// unreachable host. This error can be reported by sendto() and even by
// recvfrom(). Node should not propagate this error to the user.

var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

var socket = dgram.createSocket('udp4');
var buf = Buffer([1,2,3,4]);

function ok() {}
socket.send(buf, 0, 0, common.PORT, '127.0.0.1', ok); // useful? no
socket.send(buf, 0, 4, common.PORT, '127.0.0.1', ok);
socket.send(buf, 1, 3, common.PORT, '127.0.0.1', ok);
socket.send(buf, 3, 1, common.PORT, '127.0.0.1', ok);

assert.throws(function() {
  socket.send(buf, 0, 5, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send(buf, 2, 3, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send(buf, 4, 0, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send(buf, 4, 4, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send('abc', 4, 1, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send('abc', 0, 4, common.PORT, '127.0.0.1', assert.fail);
});
assert.throws(function() {
  socket.send('abc', -1, 2, common.PORT, '127.0.0.1', assert.fail);
});

socket.close(); // FIXME should not be necessary
