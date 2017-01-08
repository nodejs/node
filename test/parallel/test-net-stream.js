'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const s = new net.Stream();

// test that destroy called on a stream with a server only ever decrements the
// server connection count once

s.server = new net.Server();
s.server.connections = 10;
s._server = s.server;

assert.strictEqual(10, s.server.connections);
s.destroy();
assert.strictEqual(9, s.server.connections);
s.destroy();
assert.strictEqual(9, s.server.connections);

const SIZE = 2E6;
const N = 10;
const buf = Buffer.alloc(SIZE, 'a');

const server = net.createServer(function(socket) {
  socket.setNoDelay();

  socket.on('error', function(err) {
    socket.destroy();
  }).on('close', function() {
    server.close();
  });

  for (let i = 0; i < N; ++i) {
    socket.write(buf, function() { });
  }
  socket.end();

}).listen(0, function() {
  const conn = net.connect(this.address().port);
  conn.on('data', function(buf) {
    conn.pause();
    setTimeout(function() {
      conn.destroy();
    }, 20);
  });
});

process.on('exit', function() {
  assert.strictEqual(server.connections, 0);
});
