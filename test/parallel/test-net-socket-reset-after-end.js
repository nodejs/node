'use strict';

// resetAndDestroy() right after end() fails with EINVAL. The socket must
// still close.

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(common.mustCall((socket) => {
  socket.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EINVAL');
    assert.strictEqual(err.syscall, 'reset');
  }));
  socket.on('close', common.mustCall(() => {
    assert.strictEqual(socket.destroyed, true);
  }));

  socket.end('bye');
  socket.resetAndDestroy();
  server.close();
}));

server.listen(0, common.mustCall(() => {
  const client = net.connect(server.address().port);
  client.resume();
  client.on('close', common.mustCall());
}));
