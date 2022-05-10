'use strict';
const common = require('../common');

// This test ensures that socket.connect can be called without callback
// which is optional.

const net = require('net');

const server = net.createServer(common.mustCall(function(conn) {
  conn.end();
  server.close();
})).listen(0, common.mustCall(function() {
  const client = new net.Socket();

  client.on('connect', common.mustCall(function() {
    client.end();
  }));

  const address = server.address();
  if (!common.hasIPv6 && address.family === 'IPv6') {
    // Necessary to pass CI running inside containers.
    client.connect(address.port);
  } else {
    client.connect(address);
  }
}));
