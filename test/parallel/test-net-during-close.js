'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(socket) {
  socket.end();
});

server.listen(0, common.mustCall(function() {
  const client = net.createConnection(this.address().port);
  server.close();
  // server connection event has not yet fired
  // client is still attempting to connect
  assert.doesNotThrow(function() {
    client.remoteAddress;
    client.remoteFamily;
    client.remotePort;
  });
  // exit now, do not wait for the client error event
  process.exit(0);
}));
