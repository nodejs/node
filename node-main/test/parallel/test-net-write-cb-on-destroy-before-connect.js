'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer();
server.listen(0, common.mustCall(() => {
  const socket = new net.Socket();

  socket.on('connect', common.mustNotCall());

  socket.connect({
    port: server.address().port,
  });

  assert(socket.connecting);

  socket.write('foo', common.expectsError({
    code: 'ERR_SOCKET_CLOSED_BEFORE_CONNECTION',
    name: 'Error'
  }));

  socket.destroy();
  server.close();
}));
