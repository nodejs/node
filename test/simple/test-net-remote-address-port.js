var common = require('../common');
var assert = require('assert');

var net = require('net');

var conns = 0, conns_closed = 0;

var server = net.createServer(function(socket) {
  conns++;
  assert.equal('127.0.0.1', socket.remoteAddress);
  socket.on('end', function() {
    if (++conns_closed == 2) server.close();
  });
});

server.listen(common.PORT, 'localhost', function() {
  var client = net.createConnection(common.PORT, 'localhost');
  var client2 = net.createConnection(common.PORT);
  client.on('connect', function() {
    assert.equal('127.0.0.1', client.remoteAddress);
    assert.equal(common.PORT, client.remotePort);
    client.end();
  });
  client2.on('connect', function() {
    assert.equal('127.0.0.1', client2.remoteAddress);
    assert.equal(common.PORT, client2.remotePort);
    client2.end();
  });
});

process.on('exit', function() {
  assert.equal(2, conns);
});
