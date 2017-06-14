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
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: Error,
    message: /^Could not get or set buffer size:.*$/
  }));

  assert.throws(() => {
    socket.setSendBufferSize(8192);
  }, common.expectsError({
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: Error,
    message: /^Could not get or set buffer size:.*$/
  }));

  assert.throws(() => {
    socket.getRecvBufferSize();
  }, common.expectsError({
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: Error,
    message: /^Could not get or set buffer size:.*$/
  }));

  assert.throws(() => {
    socket.getSendBufferSize();
  }, common.expectsError({
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: Error,
    message: /^Could not get or set buffer size:.*$/
  }));
}

{
  // Should throw error if invalid buffer size is specified
  const socket = dgram.createSocket('udp4');

  socket.bind(common.mustCall(() => {
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

  // Can set and get buffer sizes after binding the socket.
{
  const socket = dgram.createSocket('udp4');

  socket.bind(common.mustCall(() => {
    socket.setRecvBufferSize(1200);
    socket.setSendBufferSize(1500);

    // note: linux will double the buffer size
    assert.ok(socket.getRecvBufferSize() === 1200 ||
              socket.getRecvBufferSize() === 2400,
              "SO_RCVBUF not 1200 or 2400");
    assert.ok(socket.getSendBufferSize() === 1500 ||
              socket.getSendBufferSize() === 3000,
              "SO_SNDVBUF not 1500 or 3000");
    socket.close();
  }));
}
