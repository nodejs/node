// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const { SystemError } = require('internal/errors');
const { internalBinding } = require('internal/test/binding');
const {
  UV_EBADF,
  UV_EINVAL,
  UV_ENOTSOCK
} = internalBinding('uv');

function getExpectedError(type) {
  const code = common.isWindows ? 'ENOTSOCK' : 'EBADF';
  const message = common.isWindows ?
    'socket operation on non-socket' : 'bad file descriptor';
  const errno = common.isWindows ? UV_ENOTSOCK : UV_EBADF;
  const syscall = `uv_${type}_buffer_size`;
  const suffix = common.isWindows ?
    'ENOTSOCK (socket operation on non-socket)' : 'EBADF (bad file descriptor)';
  const error = {
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: SystemError,
    message: `Could not get or set buffer size: ${syscall} returned ${suffix}`,
    info: {
      code,
      message,
      errno,
      syscall
    }
  };
  return error;
}

{
  // Should throw error if the socket is never bound.
  const errorObj = getExpectedError('send');

  const socket = dgram.createSocket('udp4');

  common.expectsError(() => {
    socket.setSendBufferSize(8192);
  }, errorObj);

  common.expectsError(() => {
    socket.getSendBufferSize();
  }, errorObj);
}

{
  const socket = dgram.createSocket('udp4');

  // Should throw error if the socket is never bound.
  const errorObj = getExpectedError('recv');

  common.expectsError(() => {
    socket.setRecvBufferSize(8192);
  }, errorObj);

  common.expectsError(() => {
    socket.getRecvBufferSize();
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

{
  const info = {
    code: 'EINVAL',
    message: 'invalid argument',
    errno: UV_EINVAL,
    syscall: 'uv_recv_buffer_size'
  };
  const errorObj = {
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: SystemError,
    message: 'Could not get or set buffer size: uv_recv_buffer_size ' +
             'returned EINVAL (invalid argument)',
    info
  };
  const socket = dgram.createSocket('udp4');
  socket.bind(common.mustCall(() => {
    common.expectsError(() => {
      socket.setRecvBufferSize(2147483648);
    }, errorObj);
    socket.close();
  }));
}

{
  const info = {
    code: 'EINVAL',
    message: 'invalid argument',
    errno: UV_EINVAL,
    syscall: 'uv_send_buffer_size'
  };
  const errorObj = {
    code: 'ERR_SOCKET_BUFFER_SIZE',
    type: SystemError,
    message: 'Could not get or set buffer size: uv_send_buffer_size ' +
             'returned EINVAL (invalid argument)',
    info
  };
  const socket = dgram.createSocket('udp4');
  socket.bind(common.mustCall(() => {
    common.expectsError(() => {
      socket.setSendBufferSize(2147483648);
    }, errorObj);
    socket.close();
  }));
}
