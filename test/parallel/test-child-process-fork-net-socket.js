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
const {
  mustCall,
  mustCallAtLeast,
} = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');
const debug = require('util').debuglog('test');

if (process.argv[2] === 'child') {

  const onSocket = mustCall((msg, socket) => {
    if (msg.what !== 'socket') return;
    process.removeListener('message', onSocket);
    socket.end('echo');
    debug('CHILD: got socket');
  });

  process.on('message', onSocket);

  process.send({ what: 'ready' });
} else {

  const child = fork(process.argv[1], ['child']);

  child.on('exit', mustCall((code, signal) => {
    const message = `CHILD: died with ${code}, ${signal}`;
    assert.strictEqual(code, 0, message);
  }));

  // Send net.Socket to child.
  function testSocket() {

    // Create a new server and connect to it,
    // but the socket will be handled by the child.
    const server = net.createServer();
    server.on('connection', mustCall((socket) => {
      // TODO(@jasnell): Close does not seem to actually be called.
      // It is not clear if it is needed.
      socket.on('close', () => {
        debug('CLIENT: socket closed');
      });
      child.send({ what: 'socket' }, socket);
    }));
    server.on('close', mustCall(() => {
      debug('PARENT: server closed');
    }));

    server.listen(0, mustCall(() => {
      debug('testSocket, listening');
      const connect = net.connect(server.address().port);
      let store = '';
      connect.on('data', mustCallAtLeast((chunk) => {
        store += chunk;
        debug('CLIENT: got data');
      }));
      connect.on('close', mustCall(() => {
        debug('CLIENT: closed');
        assert.strictEqual(store, 'echo');
        server.close();
      }));
    }));
  }

  const onReady = mustCall((msg) => {
    if (msg.what !== 'ready') return;
    child.removeListener('message', onReady);

    testSocket();
  });

  // Create socket and send it to child.
  child.on('message', onReady);
}
