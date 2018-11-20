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

const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');

if (cluster.isMaster)
  master();
else
  worker();


function master() {
  let listening = 0;

  // Fork 4 workers.
  for (let i = 0; i < NUM_WORKERS; i++)
    cluster.fork();

  // Wait until all workers are listening.
  cluster.on('listening', common.mustCall((worker, address) => {
    if (++listening < NUM_WORKERS)
      return;

    // Start sending messages.
    const buf = Buffer.from('hello world');
    const socket = dgram.createSocket('udp4');
    let sent = 0;
    doSend();

    function doSend() {
      socket.send(buf, 0, buf.length, address.port, address.address, afterSend);
    }

    function afterSend() {
      sent++;
      if (sent < NUM_WORKERS * PACKETS_PER_WORKER) {
        doSend();
      } else {
        socket.close();
      }
    }
  }, NUM_WORKERS));

  // Set up event handlers for every worker. Each worker sends a message when
  // it has received the expected number of packets. After that it disconnects.
  for (const key in cluster.workers) {
    if (cluster.workers.hasOwnProperty(key))
      setupWorker(cluster.workers[key]);
  }

  function setupWorker(worker) {
    let received = 0;

    worker.on('message', common.mustCall((msg) => {
      received = msg.received;
      worker.disconnect();
    }));

    worker.on('exit', common.mustCall(() => {
      assert.strictEqual(received, PACKETS_PER_WORKER);
    }));
  }
}


function worker() {
  let received = 0;

  // Create udp socket and start listening.
  const socket = dgram.createSocket('udp4');

  socket.on('message', common.mustCall((data, info) => {
    received++;

    // Every 10 messages, notify the master.
    if (received === PACKETS_PER_WORKER) {
      process.send({ received });
      socket.close();
    }
  }, PACKETS_PER_WORKER));

  socket.bind(0);
}
