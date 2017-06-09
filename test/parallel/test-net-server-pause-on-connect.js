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
const assert = require('assert');
const net = require('net');
const msg = 'test';
let stopped = true;
let server1Sock;


const server1ConnHandler = (socket) => {
  socket.on('data', function(data) {
    if (stopped) {
      assert.fail('data event should not have happened yet');
    }

    assert.strictEqual(data.toString(), msg, 'invalid data received');
    socket.end();
    server1.close();
  });

  server1Sock = socket;
};

const server1 = net.createServer({pauseOnConnect: true}, server1ConnHandler);

const server2ConnHandler = (socket) => {
  socket.on('data', function(data) {
    assert.strictEqual(data.toString(), msg, 'invalid data received');
    socket.end();
    server2.close();

    assert.strictEqual(server1Sock.bytesRead, 0,
                       'no data should have been read yet');
    server1Sock.resume();
    stopped = false;
  });
};

const server2 = net.createServer({pauseOnConnect: false}, server2ConnHandler);

server1.listen(0, function() {
  const clientHandler = common.mustCall(function() {
    server2.listen(0, function() {
      net.createConnection({port: this.address().port}).write(msg);
    });
  });
  net.createConnection({port: this.address().port}).write(msg, clientHandler);
});

process.on('exit', function() {
  assert.strictEqual(stopped, false);
});
