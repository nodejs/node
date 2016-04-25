'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const Buffer = require('buffer').Buffer;

var server = net.createServer(function(socket) {
  socket.end();
});

server.listen(common.PORT, common.mustCall(function() {
  var socket = net.connect(common.PORT);

  // Cork the socket, then write twice; this should cause a writev, which
  // previously caused an err in the bytesWritten count.
  socket.cork();

  socket.write('one');
  socket.write(new Buffer('twø', 'utf8'));

  socket.uncork();

  // one = 3 bytes, twø = 4 bytes
  assert.equal(socket.bytesWritten, 3 + 4);

  socket.on('connect', common.mustCall(function() {
    assert.equal(socket.bytesWritten, 3 + 4);
  }));

  socket.on('end', common.mustCall(function() {
    assert.equal(socket.bytesWritten, 3 + 4);

    server.close();
  }));
}));
