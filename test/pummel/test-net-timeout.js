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

let exchanges = 0;
let starttime = null;
let timeouttime = null;
const timeout = 1000;

const echo_server = net.createServer((socket) => {
  socket.setTimeout(timeout);

  socket.on('timeout', () => {
    console.log('server timeout');
    timeouttime = new Date();
    console.dir(timeouttime);
    socket.destroy();
  });

  socket.on('error', (e) => {
    throw new Error(
      'Server side socket should not get error. We disconnect willingly.');
  });

  socket.on('data', (d) => {
    socket.write(d);
  });

  socket.on('end', () => {
    socket.end();
  });
});

echo_server.listen(0, () => {
  const port = echo_server.address().port;
  console.log(`server listening at ${port}`);

  const client = net.createConnection(port);
  client.setEncoding('UTF8');
  client.setTimeout(0); // Disable the timeout for client
  client.on('connect', () => {
    console.log('client connected.');
    client.write('hello\r\n');
  });

  client.on('data', (chunk) => {
    assert.strictEqual(chunk, 'hello\r\n');
    if (exchanges++ < 5) {
      setTimeout(() => {
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

  client.on('timeout', () => {
    throw new Error("client timeout - this shouldn't happen");
  });

  client.on('end', () => {
    console.log('client end');
    client.end();
  });

  client.on('close', () => {
    console.log('client disconnect');
    echo_server.close();
  });
});

process.on('exit', () => {
  assert.ok(starttime != null);
  assert.ok(timeouttime != null);

  const diff = timeouttime - starttime;
  console.log(`diff = ${diff}`);

  assert.ok(timeout < diff);
});
