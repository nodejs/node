'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(common.mustCall((socket) => {
  socket.end(Buffer.alloc(1024));
})).listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port);
  assert(socket.allowHalfOpen === false);
  socket.resume();
  socket.on('end', common.mustCall(() => {
    process.nextTick(() => {
      assert(!socket.destroyed);
      server.close();
    })
  }));
  socket.on('close', common.mustCall());
}));
