'use strict';
const common = require('../common');

// This test ensures that socket.connect can be called without callback
// which is optional.

const net = require('net');

const server = net.createServer(common.mustCall(function(conn) {
  conn.end();
  server.close();
})).listen(0, 'localhost', common.mustCall(function() {
  const client = new net.Socket();

  client.on('ready', common.mustCall(function() {
    client.end();
  }));

  client.connect(server.address());
}));
