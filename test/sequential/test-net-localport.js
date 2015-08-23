'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(socket) {
  console.log(socket.remotePort);
  assert.strictEqual(socket.remotePort, common.PORT + 1);
  socket.end();
  socket.on('close', function() {
    server.close();
  });
}).listen(common.PORT).on('listening', function() {
  const client = net.connect({
    host: '127.0.0.1',
    port: common.PORT,
    localPort: common.PORT + 1,
  }).on('connect', function() {
    assert.strictEqual(client.localPort, common.PORT + 1);
  });
});
