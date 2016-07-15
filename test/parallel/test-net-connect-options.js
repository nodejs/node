'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

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
    allowHalfOpen: true
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
