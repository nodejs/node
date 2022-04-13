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

// Test that unref'ed sockets with timeouts do not prevent exit.

const common = require('../common');
const net = require('net');

const server = net.createServer(function(c) {
  c.write('hello');
  c.unref();
});
server.listen(0);
server.unref();

let connections = 0;
const sockets = [];
const delays = [8, 5, 3, 6, 2, 4];

delays.forEach(function(T) {
  const socket = net.createConnection(server.address().port, 'localhost');
  socket.on('connect', common.mustCall(function() {
    if (++connections === delays.length) {
      sockets.forEach(function(s) {
        s.socket.setTimeout(s.timeout, function() {
          s.socket.destroy();
          throw new Error('socket timed out unexpectedly');
        });

        s.socket.unref();
      });
    }
  }));

  sockets.push({ socket: socket, timeout: T * 1000 });
});
