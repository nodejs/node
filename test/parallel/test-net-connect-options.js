'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var assertTimeoutCalled = false;
var orig = net.Socket.prototype.setTimeout;
net.Socket.prototype.setTimeout = function () {
  if (!assertTimeoutCalled)
    assertTimeoutCalled = arguments[0] === 0;
  orig.apply(this, arguments);
};

process.on('exit', function () {
  assert.ok(assertTimeoutCalled);
  net.Socket.prototype.setTimeout = orig;
});

var server = net.createServer({
  allowHalfOpen: true
}, common.mustCall(function(socket) {
  socket.resume();
  socket.on('end', common.mustCall(function() {}));
  socket.end();
}));

server.listen(0, function() {
  var client = net.connect({
    host: '127.0.0.1',
    port: this.address().port,
    allowHalfOpen: true,
    timeout: 0
  }, common.mustCall(function() {
    console.error('client connect cb');
    client.resume();
    client.on('end', common.mustCall(function() {
      setTimeout(function() {
        assert(client.writable);
        client.end();
      }, 10);
    }));
    client.on('close', function() {
      server.close();
    });
  }));
});
