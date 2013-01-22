// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following condonitions:
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
  var i;
  var received = 0;

  // Start listening on a socket.
  var socket = dgram.createSocket('udp4');
  socket.bind(common.PORT);

  // Disconnect workers when the expected number of messages have been
  // received.
  socket.on('message', function(data, info) {
    received++;

    if (received == PACKETS_PER_WORKER * NUM_WORKERS) {
      console.log('master received %d packets', received);

      // Close the socket.
      socket.close();

      // Disconnect all workers.
      cluster.disconnect();
    }
  });

  // Fork workers.
  for (var i = 0; i < NUM_WORKERS; i++)
    cluster.fork();
}


function worker() {
  // Create udp socket and send packets to master.
  var socket = dgram.createSocket('udp4');
  var buf = new Buffer('hello world');

  for (var i = 0; i < PACKETS_PER_WORKER; i++)
    socket.send(buf, 0, buf.length, common.PORT, '127.0.0.1');

  console.log('worker %d sent %d packets', cluster.worker.id, PACKETS_PER_WORKER);
}
