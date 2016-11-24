'use strict';
const common = require('../common');
const NUM_WORKERS = 4;
const PACKETS_PER_WORKER = 10;

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
  var received = 0;

  // Start listening on a socket.
  var socket = dgram.createSocket('udp4');
  socket.bind(common.PORT);

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

  // Fork workers.
  for (var i = 0; i < NUM_WORKERS; i++)
    cluster.fork();
}


function worker() {
  // Create udp socket and send packets to master.
  const socket = dgram.createSocket('udp4');
  const buf = Buffer.from('hello world');

  // This test is intended to exercise the cluster binding of udp sockets, but
  // since sockets aren't clustered when implicitly bound by at first call of
  // send(), explicitly bind them to an ephemeral port.
  socket.bind(0);

  // There is no guarantee that a sent dgram packet will be received so keep
  // sending until disconnect.
  const interval = setInterval(() => {
    socket.send(buf, 0, buf.length, common.PORT, '127.0.0.1');
  }, 1);

  cluster.worker.on('disconnect', () => {
    clearInterval(interval);
  });
}
