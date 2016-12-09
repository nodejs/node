'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(function(s) {
  console.error('SERVER: got connection');
  s.end();
});

server.listen(0, common.mustCall(function() {
  var c = net.createConnection(this.address().port);
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
