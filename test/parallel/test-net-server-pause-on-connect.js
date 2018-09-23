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
    // After 50(ish) ms, the other socket should have already read the data.
    assert.equal(read, true);
    assert.equal(socket.bytesRead, 0, 'no data should have been read yet');

    socket.resume();
    stopped = false;
  }, common.platformTimeout(50));
});

// read is a timing check, as server1's timer should fire after server2's
// connection receives the data. Note that this could be race-y.
var read = false;
var server2 = net.createServer({pauseOnConnect: false}, function(socket) {
  socket.on('data', function(data) {
    read = true;

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

process.on('exit', function() {
  assert.equal(stopped, false);
  assert.equal(read, true);
});
