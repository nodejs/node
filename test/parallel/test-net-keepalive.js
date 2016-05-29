'use strict';
var common = require('../common');
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
  }, common.platformTimeout(100));
  connection.setTimeout(0);
  assert.notEqual(connection.setKeepAlive, undefined);
  // send a keepalive packet after 50 ms
  connection.setKeepAlive(true, common.platformTimeout(50));
  connection.on('end', function() {
    connection.end();
  });
});
echoServer.listen(0);

echoServer.on('listening', function() {
  clientConnection = net.createConnection(this.address().port);
  clientConnection.setTimeout(0);
});
