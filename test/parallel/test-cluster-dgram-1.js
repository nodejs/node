'use strict';
const common = require('../common');
const NUM_WORKERS = 4;
const PACKETS_PER_WORKER = 10;

const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');


if (common.isWindows) {
  common.skip('dgram clustering is currently not supported ' +
              'on windows.');
  return;
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
  cluster.on('listening', common.mustCall(() => {
    if (++listening < NUM_WORKERS)
      return;

    // Start sending messages.
    const buf = Buffer.from('hello world');
    const socket = dgram.createSocket('udp4');
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
    var received = 0;

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
  var received = 0;

  // Create udp socket and start listening.
  var socket = dgram.createSocket('udp4');

  socket.on('message', common.mustCall((data, info) => {
    received++;

    // Every 10 messages, notify the master.
    if (received === PACKETS_PER_WORKER) {
      process.send({received: received});
      socket.close();
    }
  }, PACKETS_PER_WORKER));

  socket.bind(common.PORT);
}
