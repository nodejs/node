'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Linux doubles the requested socket buffer size.
const SIZE = 10000;
const expectedSize = common.isLinux ? SIZE * 2 : SIZE;

// Without a handle, or with one that does not support the option, the getters
// report the requested size, or `undefined` when nothing was requested.
for (const handle of [null, {}]) {
  const socket = new net.Socket();

  assert.strictEqual(socket.getRecvBufferSize(), undefined);
  assert.strictEqual(socket.getSendBufferSize(), undefined);

  assert.strictEqual(socket.setRecvBufferSize(SIZE), socket);
  assert.strictEqual(socket.setSendBufferSize(SIZE), socket);

  assert.strictEqual(socket.getRecvBufferSize(), SIZE);
  assert.strictEqual(socket.getSendBufferSize(), SIZE);
}

// Invalid sizes are rejected.
{
  const socket = new net.Socket();
  const expectedError = {
    code: 'ERR_SOCKET_BAD_BUFFER_SIZE',
    name: 'TypeError',
    message: /^Buffer size must be a positive integer$/,
  };

  for (const size of [-1, 0, Infinity, NaN, 'Doh!', 2 ** 32, null, {}]) {
    assert.throws(() => socket.setRecvBufferSize(size), expectedError);
    assert.throws(() => socket.setSendBufferSize(size), expectedError);
  }
}

// Invalid constructor options are rejected by the usual validators.
{
  for (const option of ['recvBufferSize', 'sendBufferSize']) {
    assert.throws(() => new net.Socket({ [option]: -1 }), {
      code: 'ERR_OUT_OF_RANGE',
    });
    assert.throws(() => new net.Socket({ [option]: 'invalid' }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
}

const server = net
  .createServer(
    common.mustCall((socket) => {
      socket.resume();
    }, 3),
  )
  .listen(
    0,
    common.mustCall(() => {
      const { port } = server.address();
      let pending = 3;

      const done = (socket) => {
        socket.end();
        socket.on('close', () => {
          if (--pending === 0) server.close();
        });
      };

      // The sizes round-trip through the operating system once connected.
      {
        const socket = net.connect(
          port,
          common.mustCall(() => {
            socket.setRecvBufferSize(SIZE);
            socket.setSendBufferSize(SIZE);

            assert.strictEqual(socket.getRecvBufferSize(), expectedSize);
            assert.strictEqual(socket.getSendBufferSize(), expectedSize);

            // A size that does not fit in a signed int is rejected by libuv.
            assert.throws(() => socket.setRecvBufferSize(2147483648), {
              code: 'ERR_SOCKET_BUFFER_SIZE',
              name: 'SystemError',
              message: /uv_recv_buffer_size returned EINVAL/,
            });
            assert.throws(() => socket.setSendBufferSize(2147483648), {
              code: 'ERR_SOCKET_BUFFER_SIZE',
              name: 'SystemError',
              message: /uv_send_buffer_size returned EINVAL/,
            });

            // A rejected size does not replace the size already in effect.
            assert.strictEqual(socket.getRecvBufferSize(), expectedSize);
            assert.strictEqual(socket.getSendBufferSize(), expectedSize);

            done(socket);
          }),
        );
      }

      // A size requested before connecting is applied to the handle.
      {
        const socket = new net.Socket();

        socket.setRecvBufferSize(SIZE);
        socket.setSendBufferSize(SIZE);

        socket.connect(
          port,
          common.mustCall(() => {
            assert.strictEqual(socket.getRecvBufferSize(), expectedSize);
            assert.strictEqual(socket.getSendBufferSize(), expectedSize);

            done(socket);
          }),
        );
      }

      // The constructor options take the same path.
      {
        const socket = net.connect(
          {
            port,
            recvBufferSize: SIZE,
            sendBufferSize: SIZE,
          },
          common.mustCall(() => {
            assert.strictEqual(socket.getRecvBufferSize(), expectedSize);
            assert.strictEqual(socket.getSendBufferSize(), expectedSize);

            done(socket);
          }),
        );
      }
    }),
  );
