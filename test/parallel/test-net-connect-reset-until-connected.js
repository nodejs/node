'use strict';

const common = require('../common');
const net = require('net');

const server = net.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  const conn = net.createConnection(port);
  conn.on('close', common.mustCall());
  server.on('connection', (socket) => {
    socket.on('error', common.expectsError({
      code: 'ECONNRESET',
      message: 'read ECONNRESET',
      name: 'Error'
    }));
    server.close();
  });
  conn.resetAndDestroy();
}));
