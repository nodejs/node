'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  // Should throw EBADF if the socket is never bound.
  const socket = dgram.createSocket('udp4');

  assert.throws(() => {
    socket.setBroadcast(true);
  }, /^Error: setBroadcast EBADF$/);
}

{
  // Can call setBroadcast() after binding the socket.
  const socket = dgram.createSocket('udp4');

  socket.bind(0, common.mustCall(() => {
    socket.setBroadcast(true);
    socket.setBroadcast(false);
    socket.close();
  }));
}
