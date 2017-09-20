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
const cluster = require('cluster');
const net = require('net');

function forEach(obj, fn) {
  Object.keys(obj).forEach(function(name, index) {
    fn(obj[name], name);
  });
}

if (cluster.isWorker) {
  // Create a tcp server. This will be used as cluster-shared-server and as an
  // alternative IPC channel.
  const server = net.Server();
  let socket, message;

  function maybeReply() {
    if (!socket || !message) return;

    // Tell master using TCP socket that a message is received.
    socket.write(JSON.stringify({
      code: 'received message',
      echo: message
    }));
  }

  server.on('connection', function(socket_) {
    socket = socket_;
    maybeReply();

    // Send a message back over the IPC channel.
    process.send('message from worker');
  });

  process.on('message', function(message_) {
    message = message_;
    maybeReply();
  });

  server.listen(0, '127.0.0.1');
} else if (cluster.isMaster) {

  const checks = {
    global: {
      'receive': false,
      'correct': false
    },
    master: {
      'receive': false,
      'correct': false
    },
    worker: {
      'receive': false,
      'correct': false
    }
  };


  let client;
  const check = (type, result) => {
    checks[type].receive = true;
    checks[type].correct = result;
    console.error('check', checks);

    let missing = false;
    forEach(checks, function(type) {
      if (type.receive === false) missing = true;
    });

    if (missing === false) {
      console.error('end client');
      client.end();
    }
  };

  // Spawn worker
  const worker = cluster.fork();

  // When a IPC message is received from the worker
  worker.on('message', function(message) {
    check('master', message === 'message from worker');
  });
  cluster.on('message', function(worker_, message) {
    assert.strictEqual(worker_, worker);
    check('global', message === 'message from worker');
  });

  // When a TCP server is listening in the worker connect to it
  worker.on('listening', function(address) {

    client = net.connect(address.port, function() {
      // Send message to worker.
      worker.send('message from master');
    });

    client.on('data', function(data) {
      // All data is JSON
      data = JSON.parse(data.toString());

      if (data.code === 'received message') {
        check('worker', data.echo === 'message from master');
      } else {
        throw new Error(`wrong TCP message received: ${data}`);
      }
    });

    // When the connection ends kill worker and shutdown process
    client.on('end', function() {
      worker.kill();
    });

    worker.on('exit', function() {
      process.exit(0);
    });
  });

  process.once('exit', function() {
    forEach(checks, function(check, type) {
      assert.ok(check.receive, `The ${type} did not receive any message`);
      assert.ok(check.correct, `The ${type} did not get the correct message`);
    });
  });
}
