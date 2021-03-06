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

let destroyed;
let success;
let worker;
let server;

// Workers do not exit on disconnect, they exit under normal node rules: when
// they have nothing keeping their loop alive, like an active connection
//
// test this by:
//
// 1 creating a server, so worker can make a connection to something
// 2 disconnecting worker
// 3 wait to confirm it did not exit
// 4 destroy connection
// 5 confirm it does exit
if (cluster.isPrimary) {
  server = net.createServer(function(conn) {
    server.close();
    worker.disconnect();
    worker.once('disconnect', function() {
      setTimeout(function() {
        conn.destroy();
        destroyed = true;
      }, 1000);
    }).once('exit', function() {
      // Worker should not exit while it has a connection
      assert(destroyed, 'worker exited before connection destroyed');
      success = true;
    });

  }).listen(0, function() {
    const port = this.address().port;

    worker = cluster.fork()
      .on('online', function() {
        this.send({ port });
      });
  });
  process.on('exit', function() {
    assert(success);
  });
} else {
  process.on('message', function(msg) {
    // We shouldn't exit, not while a network connection exists
    net.connect(msg.port);
  });
}
