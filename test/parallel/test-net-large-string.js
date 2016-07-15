'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

var kPoolSize = 40 * 1024;
var data = 'あ'.repeat(kPoolSize);
var encoding = 'UTF-8';

var server = net.createServer(common.mustCall(function(socket) {
  var receivedSize = 0;

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
  var client = net.createConnection(this.address().port);
  client.on('end', function() {
    server.close();
  });
  client.write(data, encoding);
  client.end();
});
