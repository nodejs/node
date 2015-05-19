'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var msg = 'test';
var stopped = true;
var server1 = net.createServer({pauseOnConnect: true}, function(socket) {
  socket.on('data', function(data) {
    if (stopped) {
      assert(false, 'data event should not have happened yet');
    }

    assert.equal(data.toString(), msg, 'invalid data received');
    socket.end();
    server1.close();
  });

  setTimeout(function() {
    assert.equal(socket.bytesRead, 0, 'no data should have been read yet');
    socket.resume();
    stopped = false;
  }, 3000);
});

var server2 = net.createServer({pauseOnConnect: false}, function(socket) {
  socket.on('data', function(data) {
    assert.equal(data.toString(), msg, 'invalid data received');
    socket.end();
    server2.close();
  });
});

server1.listen(common.PORT, function() {
  net.createConnection({port: common.PORT}).write(msg);
});

server2.listen(common.PORT + 1, function() {
  net.createConnection({port: common.PORT + 1}).write(msg);
});
