'use strict';

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (cluster.isWorker) {
  net.createServer(function(socket) {
    // Wait for any data, then close connection
    socket.on('data', socket.end.bind(socket));
  }).listen(common.PORT, common.localhostIPv4);
} else if (cluster.isMaster) {

  var connectionDone;
  var ok;

  // start worker
  var worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', function() {
    net.createConnection(common.PORT, common.localhostIPv4, function() {
      var socket = this;
      worker.disconnect();
      setTimeout(function() {
        socket.write('.');
        connectionDone = true;
      }, 1000);
    });
  });

  // Check worker events and properties
  worker.once('disconnect', function() {
    assert.ok(connectionDone, 'disconnect should occur after socket close');
    ok = true;
  });

  process.once('exit', function() {
    assert.ok(ok);
  });
}
