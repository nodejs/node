'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(common.mustCall((socket) => {
  socket.end(Buffer.alloc(1024));
})).listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port);
  assert.strictEqual(socket.allowHalfOpen, false);
  socket.resume();
  socket.on('end', common.mustCall(() => {
    process.nextTick(() => {
      // Ensure socket is not destroyed straight away
      // without proper shutdown.
      assert(!socket.destroyed);
      server.close();
    });
  }));
  socket.on('finish', common.mustCall(() => {
    assert(!socket.destroyed);
  }));
  socket.on('close', common.mustCall());
}));
