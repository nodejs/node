'use strict';
var common = require('../common');
var assert = require('assert');

var net = require('net');

var s = new net.Stream();

// test that destroy called on a stream with a server only ever decrements the
// server connection count once

s.server = new net.Server();
s.server.connections = 10;

assert.equal(10, s.server.connections);
s.destroy();
assert.equal(9, s.server.connections);
s.destroy();
assert.equal(9, s.server.connections);

var SIZE = 2E6;
var N = 10;
var buf = new Buffer(SIZE);
buf.fill(0x61); // 'a'

var server = net.createServer(function(socket) {
  socket.setNoDelay();

  socket.on('error', function(err) {
    socket.destroy();
  }).on('close', function() {
    server.close();
  });

  for (var i = 0; i < N; ++i) {
    socket.write(buf, function() { });
  }
  socket.end();

}).listen(common.PORT, function() {
    var conn = net.connect(common.PORT);
    conn.on('data', function(buf) {
      conn.pause();
      setTimeout(function() {
        conn.destroy();
      }, 20);
    });
  });

process.on('exit', function() {
  assert.equal(server.connections, 0);
});
