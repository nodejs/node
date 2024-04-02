'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(socket) {
  assert.strictEqual(socket.remotePort, common.PORT);
  socket.end();
  socket.on('close', function() {
    server.close();
  });
}).listen(0).on('listening', function() {
  const client = net.connect({
    host: '127.0.0.1',
    port: this.address().port,
    localPort: common.PORT,
  }).on('connect', function() {
    assert.strictEqual(client.localPort, common.PORT);
  });
});
