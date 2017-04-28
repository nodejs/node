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

// progress tracker
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
      process.send({what: 'connection'});
      socket.destroy();
    });

    // start making connection from parent
    console.log('CHILD: server listening');
    process.send({what: 'listening'});
  });

  process.on('message', function onClose(msg) {
    if (msg.what !== 'close') return;
    process.removeListener('message', onClose);

    serverScope.on('close', function() {
      process.send({what: 'close'});
    });
    serverScope.close();
  });

  process.on('message', function onSocket(msg, socket) {
    if (msg.what !== 'socket') return;
    process.removeListener('message', onSocket);
    socket.end('echo');
    console.log('CHILD: got socket');
  });

  process.send({what: 'ready'});
} else {

  const child = fork(process.argv[1], ['child']);

  child.on('exit', common.mustCall(function(code, signal) {
    const message = `CHILD: died with ${code}, ${signal}`;
    assert.strictEqual(code, 0, message);
  }));

  // send net.Server to child and test by connecting
  function testServer(callback) {

    // destroy server execute callback when done
    const progress = new ProgressTracker(2, function() {
      server.on('close', function() {
        console.log('PARENT: server closed');
        child.send({what: 'close'});
      });
      server.close();
    });

    // we expect 4 connections and close events
    const connections = new ProgressTracker(4, progress.done.bind(progress));
    const closed = new ProgressTracker(4, progress.done.bind(progress));

    // create server and send it to child
    const server = net.createServer();
    server.on('connection', function(socket) {
      console.log('PARENT: got connection');
      socket.destroy();
      connections.done();
    });
    server.on('listening', function() {
      console.log('PARENT: server listening');
      child.send({what: 'server'}, server);
    });
    server.listen(0);

    // handle client messages
    function messageHandlers(msg) {

      if (msg.what === 'listening') {
        // make connections
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

  // send net.Socket to child
  function testSocket(callback) {

    // create a new server and connect to it,
    // but the socket will be handled by the child
    const server = net.createServer();
    server.on('connection', function(socket) {
      socket.on('close', function() {
        console.log('CLIENT: socket closed');
      });
      child.send({what: 'socket'}, socket);
    });
    server.on('close', function() {
      console.log('PARENT: server closed');
      callback();
    });
    // don't listen on the same port, because SmartOS sometimes says
    // that the server's fd is closed, but it still cannot listen
    // on the same port again.
    //
    // An isolated test for this would be lovely, but for now, this
    // will have to do.
    server.listen(0, function() {
      console.error('testSocket, listening');
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

  // create server and send it to child
  let serverSuccess = false;
  let socketSuccess = false;
  child.on('message', function onReady(msg) {
    if (msg.what !== 'ready') return;
    child.removeListener('message', onReady);

    testServer(function() {
      serverSuccess = true;

      testSocket(function() {
        socketSuccess = true;
      });
    });

  });

  process.on('exit', function() {
    assert.ok(serverSuccess);
    assert.ok(socketSuccess);
  });

}
