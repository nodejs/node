'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(common.mustCall(function(s) {
  server.close();
  s.end();
})).listen(0, 'localhost', common.mustCall(function() {
  const socket = net.connect(this.address().port, 'localhost');
  socket.on('end', common.mustCall(() => {
    assert.strictEqual(socket.writable, true);
    socket.write('hello world');
  }));
}));
