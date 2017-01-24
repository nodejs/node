'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(s) {
  console.error('SERVER: got connection');
  s.end();
});

server.listen(0, common.mustCall(function() {
  const c = net.createConnection(this.address().port);
  c.on('close', common.mustCall(function() {
    console.error('connection closed');
    assert.strictEqual(c._handle, null);
    assert.doesNotThrow(function() {
      c.setNoDelay();
      c.setKeepAlive();
      c.bufferSize;
      c.pause();
      c.resume();
      c.address();
      c.remoteAddress;
      c.remotePort;
    });
    server.close();
  }));
}));
