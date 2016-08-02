'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    socket.setMulticastInterface('0.0.0.0');

    // close the socket
    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {

    // Try to set with an invalid interface address (wrong address class)
    assert.throws(common.mustCall(() => {
      socket.setMulticastInterface('::');
    }));

    // close the socket
    socket.close();
  }));
}
