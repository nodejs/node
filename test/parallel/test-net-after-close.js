'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var closed = false;

var server = net.createServer(function(s) {
  console.error('SERVER: got connection');
  s.end();
});

server.listen(common.PORT, function() {
  var c = net.createConnection(common.PORT);
  c.on('close', function() {
    console.error('connection closed');
    assert.strictEqual(c._handle, null);
    closed = true;
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
  });
});

process.on('exit', function() {
  assert(closed);
});
