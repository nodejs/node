'use strict';

const common = require('../common');
const net = require('net');
const { strictEqual } = require('assert');

const server = net.createServer();

server.listen(common.mustCall(function() {
  const socket = net.connect({ port: server.address().port });

  strictEqual(socket.connecting, true);
  strictEqual(socket.remoteAddress, undefined);

  socket.on('connect', common.mustCall(function() {
    strictEqual(socket.remoteAddress !== undefined, true);
    socket.end();
  }));

  socket.on('end', common.mustCall(function() {
    server.close();
  }));
}));
