'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

var SIZE = 2E5;
var N = 10;
var flushed = 0;
var received = 0;
var buf = Buffer.alloc(SIZE, 'a');

var server = net.createServer(function(socket) {
  socket.setNoDelay();
  socket.setTimeout(9999);
  socket.on('timeout', function() {
    common.fail(`flushed: ${flushed}, received: ${received}/${SIZE * N}`);
  });

  for (var i = 0; i < N; ++i) {
    socket.write(buf, function() {
      ++flushed;
      if (flushed === N) {
        socket.setTimeout(0);
      }
    });
  }
  socket.end();

}).listen(0, common.mustCall(function() {
  var conn = net.connect(this.address().port);
  conn.on('data', function(buf) {
    received += buf.length;
    conn.pause();
    setTimeout(function() {
      conn.resume();
    }, 20);
  });
  conn.on('end', common.mustCall(function() {
    server.close();
    assert.strictEqual(received, SIZE * N);
  }));
}));
