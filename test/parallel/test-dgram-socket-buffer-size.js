'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  // Should throw error if the socket is never bound.
  const errorObj = {
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: Error,
    message: /^Could not get or set buffer size:.*$/
  };

  const socket = dgram.createSocket('udp4');

  common.expectsError(() => {
    socket.setRecvBufferSize(8192);
  }, errorObj);

  common.expectsError(() => {
    socket.setSendBufferSize(8192);
  }, errorObj);

  common.expectsError(() => {
    socket.getRecvBufferSize();
  }, errorObj);

  common.expectsError(() => {
    socket.getSendBufferSize();
  }, errorObj);
}

{
  // Should throw error if invalid buffer size is specified
  const errorObj = {
    code: 'ERR_SOCKET_BAD_BUFFER_SIZE',
    type: TypeError,
    message: /^Buffer size must be a positive integer$/
  };

  const badBufferSizes = [-1, Infinity, 'Doh!'];

  const socket = dgram.createSocket('udp4');

  socket.bind(common.mustCall(() => {
    badBufferSizes.forEach((badBufferSize) => {
      common.expectsError(() => {
        socket.setRecvBufferSize(badBufferSize);
      }, errorObj);

      common.expectsError(() => {
        socket.setSendBufferSize(badBufferSize);
      }, errorObj);
    });
    socket.close();
  }));
}

{
  // Can set and get buffer sizes after binding the socket.
  const socket = dgram.createSocket('udp4');

  socket.bind(common.mustCall(() => {
    socket.setRecvBufferSize(10000);
    socket.setSendBufferSize(10000);

    // note: linux will double the buffer size
    const expectedBufferSize = common.isLinux ? 20000 : 10000;
    assert.strictEqual(socket.getRecvBufferSize(), expectedBufferSize);
    assert.strictEqual(socket.getSendBufferSize(), expectedBufferSize);
    socket.close();
  }));
}

function checkBufferSizeError(type, size) {
  const errorObj = {
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: Error,
    message: /^Could not get or set buffer size:.*$/
  };
  const functionName = `set${type.charAt(0).toUpperCase()}${type.slice(1)}` +
    'BufferSize';
  const socket = dgram.createSocket('udp4');
  socket.bind(common.mustCall(() => {
    common.expectsError(() => {
      socket[functionName](size);
    }, errorObj);
    socket.close();
  }));
}

checkBufferSizeError('recv', 2147483648);
checkBufferSizeError('send', 2147483648);
