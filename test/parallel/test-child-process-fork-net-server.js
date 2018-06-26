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

function ProgressTracker(missing, callback) {
  this.missing = missing;
  this.callback = callback;
}
ProgressTracker.prototype.done = function() {
  this.missing -= 1;
  this.check();
};
ProgressTracker.prototype.check = function() {
  if (this.missing === 0) this.callback();
};

if (process.argv[2] === 'child') {

  let serverScope;

  process.on('message', function onServer(msg, server) {
    if (msg.what !== 'server') return;
    process.removeListener('message', onServer);

    serverScope = server;

    server.on('connection', function(socket) {
      console.log('CHILD: got connection');
      process.send({ what: 'connection' });
      socket.destroy();
    });

    // Start making connection from parent.
    console.log('CHILD: server listening');
    process.send({ what: 'listening' });
  });

  process.on('message', function onClose(msg) {
    if (msg.what !== 'close') return;
    process.removeListener('message', onClose);

    serverScope.on('close', function() {
      process.send({ what: 'close' });
    });
    serverScope.close();
  });

  process.send({ what: 'ready' });
} else {

  const child = fork(process.argv[1], ['child']);

  child.on('exit', common.mustCall(function(code, signal) {
    const message = `CHILD: died with ${code}, ${signal}`;
    assert.strictEqual(code, 0, message);
  }));

  // Send net.Server to child and test by connecting.
  function testServer(callback) {

    // Destroy server execute callback when done.
    const progress = new ProgressTracker(2, function() {
      server.on('close', function() {
        console.log('PARENT: server closed');
        child.send({ what: 'close' });
      });
      server.close();
    });

    // We expect 4 connections and close events.
    const connections = new ProgressTracker(4, progress.done.bind(progress));
    const closed = new ProgressTracker(4, progress.done.bind(progress));

    // Create server and send it to child.
    const server = net.createServer();
    server.on('connection', function(socket) {
      console.log('PARENT: got connection');
      socket.destroy();
      connections.done();
    });
    server.on('listening', function() {
      console.log('PARENT: server listening');
      child.send({ what: 'server' }, server);
    });
    server.listen(0);

    // Handle client messages.
    function messageHandlers(msg) {

      if (msg.what === 'listening') {
        // Make connections.
        let socket;
        for (let i = 0; i < 4; i++) {
          socket = net.connect(server.address().port, function() {
            console.log('CLIENT: connected');
          });
          socket.on('close', function() {
            closed.done();
            console.log('CLIENT: closed');
          });
        }

      } else if (msg.what === 'connection') {
        // child got connection
        connections.done();
      } else if (msg.what === 'close') {
        child.removeListener('message', messageHandlers);
        callback();
      }
    }

    child.on('message', messageHandlers);
  }

  // Create server and send it to child.
  child.on('message', function onReady(msg) {
    if (msg.what !== 'ready') return;
    child.removeListener('message', onReady);

    testServer(common.mustCall());
  });
}
