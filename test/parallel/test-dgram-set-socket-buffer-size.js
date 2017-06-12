'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  // Should throw error if the socket is never bound.
  const socket = dgram.createSocket('udp4');

  assert.throws(() => {
    socket.setRecvBufferSize(8192);
  }, common.expectsError({
    code: 'ERR_SOCKET_SET_BUFFER_SIZE',
    type: Error,
    message: /^Coud not set buffer size: E[A-Z]+$/
  }));

  assert.throws(() => {
    socket.setSendBufferSize(8192);
  }, common.expectsError({
    code: 'ERR_SOCKET_SET_BUFFER_SIZE',
    type: Error,
    message: /^Coud not set buffer size: E[A-Z]+$/
  }));
  
  
}

{
  // Should throw error if invalid buffer size is specified
  const socket = dgram.createSocket('udp4');

  socket.bind(0, common.mustCall(() => {

    assert.throws(() => {
      socket.setRecvBufferSize(-1);
    }, common.expectsError({
      code: 'ERR_SOCKET_BAD_BUFFER_SIZE',
      type: Error,
      message: /^Buffer size must be a positive integer$/
    }));

    assert.throws(() => {
      socket.setRecvBufferSize(Infinity);
    }, common.expectsError({
      code: 'ERR_SOCKET_BAD_BUFFER_SIZE',
      type: Error,
      message: /^Buffer size must be a positive integer$/
    }));

    assert.throws(() => {
      socket.setRecvBufferSize('Doh!');
    }, common.expectsError({
      code: 'ERR_SOCKET_BAD_BUFFER_SIZE',
      type: Error,
      message: /^Buffer size must be a positive integer$/
    }));

    socket.close();
  }));
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
