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
// This example attempts to time out before the connection is established
// https://groups.google.com/forum/#!topic/nodejs/UE0ZbfLt6t8
// https://groups.google.com/forum/#!topic/nodejs-dev/jR7-5UDqXkw

const common = require('../common');
const net = require('net');
const assert = require('assert');

const start = new Date();

const T = 100;

// 192.0.2.1 is part of subnet assigned as "TEST-NET" in RFC 5737.
// For use solely in documentation and example source code.
// In short, it should be unreachable.
// In practice, it's a network black hole.
const socket = net.createConnection(9999, '192.0.2.1');

socket.setTimeout(T);

socket.on('timeout', common.mustCall(function() {
  console.error('timeout');
  const now = new Date();
  assert.ok(now - start < T + 500);
  socket.destroy();
}));

socket.on('connect', common.mustNotCall());
