'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(socket) {
  socket.end();
});

server.listen(0, common.mustCall(function() {
  const socket = net.connect(server.address().port);

  // Cork the socket, then write twice; this should cause a writev, which
  // previously caused an err in the bytesWritten count.
  socket.cork();

  socket.write('one');
  socket.write(new Buffer('twø', 'utf8'));

  socket.uncork();

  // one = 3 bytes, twø = 4 bytes
  assert.strictEqual(socket.bytesWritten, 3 + 4);

  socket.on('connect', common.mustCall(function() {
    assert.strictEqual(socket.bytesWritten, 3 + 4);
  }));

  socket.on('end', common.mustCall(function() {
    assert.strictEqual(socket.bytesWritten, 3 + 4);

    server.close();
  }));
}));
