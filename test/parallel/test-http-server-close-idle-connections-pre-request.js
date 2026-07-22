'use strict';

const common = require('../common');

const { createServer } = require('http');
const { createConnection } = require('net');

const server = createServer(common.mustNotCall());

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;
  server.once('connection', common.mustCall(() => {
    server.close(common.mustCall());
    server.closeIdleConnections();
    setTimeout(common.mustNotCall(), common.platformTimeout(1000)).unref();
  }));

  const socket = createConnection(port, '127.0.0.1');

  socket.on('connect', common.mustCall());
  socket.on('close', common.mustCall());
  socket.on('error', () => {});
}));
