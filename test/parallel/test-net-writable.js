'use strict';
const common = require('../common');
const assert = require('assert');

const net = require('net');

net.createServer(function(s) {
  this.close();
  s.end();
}).listen(0, 'localhost', function() {
  const socket = net.connect(this.address().port, 'localhost');
  socket.on('end', common.mustCall(() => {
    assert.strictEqual(socket.writable, true);
    socket.write('hello world');
  }));
});
