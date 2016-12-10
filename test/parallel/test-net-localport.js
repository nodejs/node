'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(function(socket) {
  console.log(socket.remotePort);
  assert.strictEqual(socket.remotePort, common.PORT);
  socket.end();
  socket.on('close', function() {
    server.close();
  });
}).listen(0).on('listening', function() {
  var client = net.connect({
    host: '127.0.0.1',
    port: this.address().port,
    localPort: common.PORT,
  }).on('connect', function() {
    assert.strictEqual(client.localPort, common.PORT);
  });
});
