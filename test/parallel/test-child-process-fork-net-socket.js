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
const fork = require('child_process').fork;
const net = require('net');

if (process.argv[2] === 'child') {

  process.on('message', function onSocket(msg, socket) {
    if (msg.what !== 'socket') return;
    process.removeListener('message', onSocket);
    socket.end('echo');
    console.log('CHILD: got socket');
  });

  process.send({ what: 'ready' });
} else {

  const child = fork(process.argv[1], ['child']);

  child.on('exit', common.mustCall(function(code, signal) {
    const message = `CHILD: died with ${code}, ${signal}`;
    assert.strictEqual(code, 0, message);
  }));

  // Send net.Socket to child.
  function testSocket(callback) {

    // Create a new server and connect to it,
    // but the socket will be handled by the child.
    const server = net.createServer();
    server.on('connection', function(socket) {
      socket.on('close', function() {
        console.log('CLIENT: socket closed');
      });
      child.send({ what: 'socket' }, socket);
    });
    server.on('close', function() {
      console.log('PARENT: server closed');
      callback();
    });

    server.listen(0, function() {
      console.log('testSocket, listening');
      const connect = net.connect(server.address().port);
      let store = '';
      connect.on('data', function(chunk) {
        store += chunk;
        console.log('CLIENT: got data');
      });
      connect.on('close', function() {
        console.log('CLIENT: closed');
        assert.strictEqual(store, 'echo');
        server.close();
      });
    });
  }

  // Create socket and send it to child.
  child.on('message', function onReady(msg) {
    if (msg.what !== 'ready') return;
    child.removeListener('message', onReady);

    testSocket(common.mustCall());
  });
}
