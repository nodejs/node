'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  // Should throw error if the socket is never bound.
  const socket = dgram.createSocket('udp4');

  assert.throws(() => {
    socket.setRecvBufferSize(8192);
  }, /^Error: setRecvBufferSize E[A-Z]+$/);
  
    assert.throws(() => {
    socket.setSendBufferSize(8192);
  }, /^Error: setSendBufferSize E[A-Z]+$/);
}

{
  // Can call setRecvBufferSize() and setRecvBufferSize() after binding the socket.
  const socket = dgram.createSocket('udp4');

  socket.bind(0, common.mustCall(() => {
    socket.setRecvBufferSize(8192);
    socket.setSendBufferSize(8192);
    socket.close();
  }));
}
