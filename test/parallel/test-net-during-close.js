'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var accessedProperties = false;

var server = net.createServer(function(socket) {
  socket.end();
});

server.listen(common.PORT, function() {
  var client = net.createConnection(common.PORT);
  server.close();
  // server connection event has not yet fired
  // client is still attempting to connect
  assert.doesNotThrow(function() {
    client.remoteAddress;
    client.remoteFamily;
    client.remotePort;
  });
  accessedProperties = true;
  // exit now, do not wait for the client error event
  process.exit(0);
});

process.on('exit', function() {
  assert(accessedProperties);
});
