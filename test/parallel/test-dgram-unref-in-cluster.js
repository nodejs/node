'use strict';
const common = require('../common');
const dgram = require('dgram');
const cluster = require('cluster');
const assert = require('assert');

if (common.isWindows)
  common.skip('dgram clustering is currently not supported on Windows.');

if (cluster.isPrimary) {
  cluster.fork();
} else {
  const socket = dgram.createSocket('udp4');
  socket.unref();
  socket.bind();
  socket.on('listening', common.mustCall(() => {
    const sockets = process.getActiveResourcesInfo().filter((item) => {
      return item === 'UDPWrap';
    });
    assert.ok(sockets.length === 0);
    process.disconnect();
  }));
}
