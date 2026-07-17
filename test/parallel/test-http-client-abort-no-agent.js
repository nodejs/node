'use strict';
const common = require('../common');
const http = require('http');
const net = require('net');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const req = http.get({
    createConnection(options, oncreate) {
      const socket = net.createConnection(options, oncreate);
      socket.once('close', () => server.close());
      return socket;
    },
    port: server.address().port
  });

  req.abort();
}));
