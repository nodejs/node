'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');

const server = net.createServer();

server.listen(common.mustCall(function() {
  const socket = net.connect({ port: server.address().port });

  assert.strictEqual(socket.connecting, true);
  assert.strictEqual(socket.remoteAddress, undefined);

  socket.on('connect', common.mustCall(function() {
    assert.strictEqual(socket.remoteAddress !== undefined, true);
    socket.end();
  }));

  socket.on('end', common.mustCall(function() {
    server.close();
  }));
}));
