'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var iter = 10;

var server = net.createServer(function(socket) {
  socket.on('readable', function() {
    socket.read();
  });

  socket.on('end', function() {
    server.close();
  });
});

server.listen(common.PORT, function() {
  var client = net.connect(common.PORT);

  client.on('finish', function() {
    assert.strictEqual(client.bufferSize, 0);
  });

  for (var i = 1; i < iter; i++) {
    client.write('a');
    assert.strictEqual(client.bufferSize, i);
  }

  client.end();
});
