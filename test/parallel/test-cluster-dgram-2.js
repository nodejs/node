'use strict';
const common = require('../common');
var NUM_WORKERS = 4;
var PACKETS_PER_WORKER = 10;

var cluster = require('cluster');
var dgram = require('dgram');


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
  var buf = Buffer.from('hello world');

  // This test is intended to exercise the cluster binding of udp sockets, but
  // since sockets aren't clustered when implicitly bound by at first call of
  // send(), explicitly bind them to an ephemeral port.
  socket.bind(0);

  for (var i = 0; i < PACKETS_PER_WORKER; i++)
    socket.send(buf, 0, buf.length, common.PORT, '127.0.0.1');

  console.log('worker %d sent %d packets', cluster.worker.id,
              PACKETS_PER_WORKER);
}
