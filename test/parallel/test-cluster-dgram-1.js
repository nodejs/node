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

var NUM_WORKERS = 4;
var PACKETS_PER_WORKER = 10;

var assert = require('assert');
var cluster = require('cluster');
var common = require('../common');
var dgram = require('dgram');


if (process.platform === 'win32') {
  console.warn("dgram clustering is currently not supported on windows.");
  process.exit(0);
}

if (cluster.isMaster)
  master();
else
  worker();


function master() {
  var listening = 0;

  // Fork 4 workers.
  for (var i = 0; i < NUM_WORKERS; i++)
    cluster.fork();

  // Wait until all workers are listening.
  cluster.on('listening', function() {
    if (++listening < NUM_WORKERS)
      return;

    // Start sending messages.
    var buf = new Buffer('hello world');
    var socket = dgram.createSocket('udp4');
    var sent = 0;
    doSend();

    function doSend() {
      socket.send(buf, 0, buf.length, common.PORT, '127.0.0.1', afterSend);
    }

    function afterSend() {
      sent++;
      if (sent < NUM_WORKERS * PACKETS_PER_WORKER) {
        doSend();
      } else {
        console.log('master sent %d packets', sent);
        socket.close();
      }
    }
  });

  // Set up event handlers for every worker. Each worker sends a message when
  // it has received the expected number of packets. After that it disconnects.
  for (var key in cluster.workers) {
    if (cluster.workers.hasOwnProperty(key))
      setupWorker(cluster.workers[key]);
  }

  function setupWorker(worker) {
    var received = 0;

    worker.on('message', function(msg) {
      received = msg.received;
      console.log('worker %d received %d packets', worker.id, received);
    });

    worker.on('disconnect', function() {
      assert(received === PACKETS_PER_WORKER);
      console.log('worker %d disconnected', worker.id);
    });
  }
}


function worker() {
  var received = 0;

  // Create udp socket and start listening.
  var socket = dgram.createSocket('udp4');

  socket.on('message', function(data, info) {
    received++;

    // Every 10 messages, notify the master.
    if (received == PACKETS_PER_WORKER) {
      process.send({received: received});
      process.disconnect();
    }
  });

  socket.bind(common.PORT);
}
