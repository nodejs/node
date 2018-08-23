// Flags: --expose-internals
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
  const { internalBinding } = require('internal/test/binding');
  const { UDP } = internalBinding('udp_wrap');

  // Create a handle and use its fd.
  const rawHandle = new UDP();
  const err = rawHandle.bind(common.localhostIPv4, 0, 0);
  assert(err >= 0, String(err));
  assert.notStrictEqual(rawHandle.fd, -1);

  const fd = rawHandle.fd;

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

    worker.send({
      fd,
    });

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

  process.on('message', common.mustCall((data) => {
    const { fd } = data;
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

    socket.bind({
      fd,
    });
  }));
}
