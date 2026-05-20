'use strict';
const common = require('../common');
const net = require('net');

const server = net.createServer();
server.listen(0);
const port = server.address().port;
const conn = net.createConnection(port);

conn.on('error', common.mustCall(() => {
  conn.resetAndDestroy();
}));

conn.on('close', common.mustCall());
server.close();
