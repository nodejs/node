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
if (common.isWindows)
  common.skip('dgram clustering is currently not supported on Windows.');

const NUM_WORKERS = 4;
const PACKETS_PER_WORKER = 10;

const cluster = require('cluster');
const dgram = require('dgram');
const assert = require('assert');

if (cluster.isPrimary)
  primary();
else
  worker();


function primary() {
  let received = 0;

  // Start listening on a socket.
  const socket = dgram.createSocket('udp4');
  socket.bind({ port: 0 }, common.mustCall(() => {

    // Fork workers.
    for (let i = 0; i < NUM_WORKERS; i++) {
      const worker = cluster.fork();
      worker.send({ port: socket.address().port });
    }
  }));

  // Disconnect workers when the expected number of messages have been
  // received.
  socket.on('message', common.mustCall((data, info) => {
    received++;

    if (received === PACKETS_PER_WORKER * NUM_WORKERS) {

      // Close the socket.
      socket.close();

      // Disconnect all workers.
      cluster.disconnect();
    }
  }, NUM_WORKERS * PACKETS_PER_WORKER));
}


function worker() {
  // Create udp socket and send packets to primary.
  const socket = dgram.createSocket('udp4');
  const buf = Buffer.from('hello world');

  // This test is intended to exercise the cluster binding of udp sockets, but
  // since sockets aren't clustered when implicitly bound by at first call of
  // send(), explicitly bind them to an ephemeral port.
  socket.bind(0);

  process.on('message', common.mustCall((msg) => {
    assert(msg.port);

    // There is no guarantee that a sent dgram packet will be received so keep
    // sending until disconnect.
    const interval = setInterval(() => {
      socket.send(buf, 0, buf.length, msg.port, '127.0.0.1');
    }, 1);

    cluster.worker.on('disconnect', () => {
      clearInterval(interval);
    });
  }));
}
