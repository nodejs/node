'use strict';

const common = require('../common');
const net = require('net');

function barrier(count, cb) {
  return function() {
    if (--count === 0)
      cb();
  };
}

const server = net.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  const conn = net.createConnection(port);
  const connok = barrier(2, () => conn.resetAndDestroy());
  conn.on('close', common.mustCall());
  server.on('connection', (socket) => {
    connok();
    socket.on('error', common.expectsError({
      code: 'ECONNRESET',
      message: 'read ECONNRESET',
      name: 'Error'
    }));
    server.close();
  });
  conn.on('connect', connok);
}));
