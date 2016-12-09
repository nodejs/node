'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var serverConnection;
var clientConnection;
var echoServer = net.createServer(function(connection) {
  serverConnection = connection;
  setTimeout(function() {
    // make sure both connections are still open
    assert.equal(serverConnection.readyState, 'open');
    assert.equal(clientConnection.readyState, 'open');
    serverConnection.end();
    clientConnection.end();
    echoServer.close();
  }, 600);
  connection.setTimeout(0);
  assert.equal(typeof connection.setKeepAlive, 'function');
  connection.on('end', function() {
    connection.end();
  });
});
echoServer.listen(0);

echoServer.on('listening', function() {
  clientConnection = new net.Socket();
  // send a keepalive packet after 1000 ms
  // and make sure it persists
  var s = clientConnection.setKeepAlive(true, 400);
  assert.ok(s instanceof net.Socket);
  clientConnection.connect(this.address().port);
  clientConnection.setTimeout(0);
});
