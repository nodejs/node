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
const common = require('../common');
const net = require('net');
const assert = require('assert');

let cbcount = 0;
const N = 500000;

const server = net.Server(function(socket) {
  socket.on('data', function(d) {
    console.error('got %d bytes', d.length);
  });

  socket.on('end', function() {
    console.error('end');
    socket.destroy();
    server.close();
  });
});

let lastCalled = -1;
function makeCallback(c) {
  let called = false;
  return function() {
    if (called)
      throw new Error(`called callback #${c} more than once`);
    called = true;
    if (c < lastCalled) {
      throw new Error(
        `callbacks out of order. last=${lastCalled} current=${c}`);
    }
    lastCalled = c;
    cbcount++;
  };
}

server.listen(common.PORT, function() {
  const client = net.createConnection(common.PORT);

  client.on('connect', function() {
    for (let i = 0; i < N; i++) {
      client.write('hello world', makeCallback(i));
    }
    client.end();
  });
});

process.on('exit', function() {
  assert.strictEqual(N, cbcount);
});
