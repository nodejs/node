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
const net = require('net');

const s = new net.Stream();

// test that destroy called on a stream with a server only ever decrements the
// server connection count once

s.server = new net.Server();
s.server.connections = 10;
s._server = s.server;

assert.strictEqual(10, s.server.connections);
s.destroy();
assert.strictEqual(9, s.server.connections);
s.destroy();
assert.strictEqual(9, s.server.connections);

const SIZE = 2E6;
const N = 10;
const buf = Buffer.alloc(SIZE, 'a');

const server = net.createServer(function(socket) {
  socket.setNoDelay();

  socket.on('error', function(err) {
    socket.destroy();
  }).on('close', function() {
    server.close();
  });

  for (let i = 0; i < N; ++i) {
    socket.write(buf, () => {});
  }
  socket.end();

}).listen(0, function() {
  const conn = net.connect(this.address().port);
  conn.on('data', function(buf) {
    assert.strictEqual(conn, conn.pause());
    setTimeout(function() {
      conn.destroy();
    }, 20);
  });
});

process.on('exit', function() {
  assert.strictEqual(server.connections, 0);
});
