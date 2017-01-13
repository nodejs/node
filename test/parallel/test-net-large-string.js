'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const kPoolSize = 40 * 1024;
const data = 'あ'.repeat(kPoolSize);
const encoding = 'UTF-8';

const server = net.createServer(common.mustCall(function(socket) {
  let receivedSize = 0;

  socket.setEncoding(encoding);
  socket.on('data', function(data) {
    receivedSize += data.length;
  });
  socket.on('end', common.mustCall(function() {
    assert.strictEqual(receivedSize, kPoolSize);
    socket.end();
  }));
}));

server.listen(0, function() {
  const client = net.createConnection(this.address().port);
  client.on('end', function() {
    server.close();
  });
  client.write(data, encoding);
  client.end();
});
