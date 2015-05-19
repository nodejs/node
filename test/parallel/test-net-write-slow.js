'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var SIZE = 2E5;
var N = 10;
var flushed = 0;
var received = 0;
var buf = new Buffer(SIZE);
buf.fill(0x61); // 'a'

var server = net.createServer(function(socket) {
  socket.setNoDelay();
  socket.setTimeout(1000);
  socket.on('timeout', function() {
    assert.fail('flushed: ' + flushed +
                ', received: ' + received + '/' + SIZE * N);
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

}).listen(common.PORT, function() {
  var conn = net.connect(common.PORT);
  conn.on('data', function(buf) {
    received += buf.length;
    conn.pause();
    setTimeout(function() {
      conn.resume();
    }, 20);
  });
  conn.on('end', function() {
    server.close();
  });
});

process.on('exit', function() {
  assert.equal(received, SIZE * N);
});
