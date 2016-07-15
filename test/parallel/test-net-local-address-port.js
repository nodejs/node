'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(common.mustCall(function(socket) {
  assert.equal(socket.localAddress, common.localhostIPv4);
  assert.equal(socket.localPort, this.address().port);
  socket.on('end', function() {
    server.close();
  });
  socket.resume();
}));

server.listen(0, common.localhostIPv4, function() {
  var client = net.createConnection(this.address().port, common.localhostIPv4);
  client.on('connect', function() {
    client.end();
  });
});
