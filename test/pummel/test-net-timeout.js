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

let exchanges = 0;
let starttime = null;
let timeouttime = null;
const timeout = 1000;

const echo_server = net.createServer(function(socket) {
  socket.setTimeout(timeout);

  socket.on('timeout', function() {
    console.log('server timeout');
    timeouttime = new Date();
    console.dir(timeouttime);
    socket.destroy();
  });

  socket.on('error', function(e) {
    throw new Error(
      'Server side socket should not get error. We disconnect willingly.');
  });

  socket.on('data', function(d) {
    console.log(d);
    socket.write(d);
  });

  socket.on('end', function() {
    socket.end();
  });
});

echo_server.listen(common.PORT, function() {
  console.log(`server listening at ${common.PORT}`);

  const client = net.createConnection(common.PORT);
  client.setEncoding('UTF8');
  client.setTimeout(0); // disable the timeout for client
  client.on('connect', function() {
    console.log('client connected.');
    client.write('hello\r\n');
  });

  client.on('data', function(chunk) {
    assert.strictEqual('hello\r\n', chunk);
    if (exchanges++ < 5) {
      setTimeout(function() {
        console.log('client write "hello"');
        client.write('hello\r\n');
      }, 500);

      if (exchanges === 5) {
        console.log(`wait for timeout - should come in ${timeout} ms`);
        starttime = new Date();
        console.dir(starttime);
      }
    }
  });

  client.on('timeout', function() {
    throw new Error("client timeout - this shouldn't happen");
  });

  client.on('end', function() {
    console.log('client end');
    client.end();
  });

  client.on('close', function() {
    console.log('client disconnect');
    echo_server.close();
  });
});

process.on('exit', function() {
  assert.ok(starttime != null);
  assert.ok(timeouttime != null);

  const diff = timeouttime - starttime;
  console.log(`diff = ${diff}`);

  assert.ok(timeout < diff);

  // Allow for 800 milliseconds more
  assert.ok(diff < timeout + 800);
});
