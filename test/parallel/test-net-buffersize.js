'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const iter = 10;

const server = net.createServer(function(socket) {
  socket.on('readable', function() {
    socket.read();
  });

  socket.on('end', function() {
    server.close();
  });
});

server.listen(0, function() {
  const client = net.connect(this.address().port);

  client.on('finish', function() {
    assert.strictEqual(client.bufferSize, 0);
  });

  for (let i = 1; i < iter; i++) {
    client.write('a');
    assert.strictEqual(client.bufferSize, i);
  }

  client.end();
});
