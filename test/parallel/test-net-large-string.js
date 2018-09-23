'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var kPoolSize = 40 * 1024;
var data = '';
for (var i = 0; i < kPoolSize; ++i) {
  data += 'あ'; // 3bytes
}
var receivedSize = 0;
var encoding = 'UTF-8';

var server = net.createServer(function(socket) {
  socket.setEncoding(encoding);
  socket.on('data', function(data) {
    receivedSize += data.length;
  });
  socket.on('end', function() {
    socket.end();
  });
});

server.listen(common.PORT, function() {
  var client = net.createConnection(common.PORT);
  client.on('end', function() {
    server.close();
  });
  client.write(data, encoding);
  client.end();
});

process.on('exit', function() {
  assert.equal(receivedSize, kPoolSize);
});
