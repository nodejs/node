'use strict';
// Ensure that closing dgram sockets in 'listening' callbacks of cluster workers
// won't throw errors.

const common = require('../common');
const dgram = require('dgram');
const cluster = require('cluster');
if (common.isWindows)
  common.skip('dgram clustering is currently not supported on windows.');

if (cluster.isPrimary) {
  for (let i = 0; i < 3; i += 1) {
    cluster.fork();
  }
} else {
  const socket = dgram.createSocket('udp4');

  socket.on('error', common.mustNotCall());

  socket.on('listening', common.mustCall(() => {
    socket.close();
  }));

  socket.on('close', common.mustCall(() => {
    cluster.worker.disconnect();
  }));

  socket.bind(0);
}
