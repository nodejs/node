'use strict';

const common = require('../common');

const net = require('net');

const server = net.createServer();

server.listen(common.mustCall(() => {
  const socket = net.createConnection(server.address().port);
  socket.on('close', common.mustCall(() => server.close()));
  socket.end();
}));
