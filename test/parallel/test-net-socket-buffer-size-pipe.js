'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const SIZE = 10000;

const server = net.createServer(common.mustCall((socket) => {
  socket.end();
})).listen(common.PIPE, common.mustCall(() => {
  if (common.isWindows) {
    // Windows named pipes are not sockets, so libuv cannot set the option.
    const socket = net.connect({ path: common.PIPE, recvBufferSize: SIZE });

    socket.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_SOCKET_BUFFER_SIZE');
      assert.strictEqual(err.info.code, 'ENOTSUP');

      socket.destroy();
      server.close();
    }));
    return;
  }

  const socket = net.connect({
    path: common.PIPE,
    recvBufferSize: SIZE,
    sendBufferSize: SIZE,
  }, common.mustCall(() => {
    // Linux doubles the requested socket buffer size.
    const expectedSize = common.isLinux ? SIZE * 2 : SIZE;

    assert.strictEqual(socket.getRecvBufferSize(), expectedSize);
    assert.strictEqual(socket.getSendBufferSize(), expectedSize);

    socket.destroy();
    server.close();
  }));
}));
