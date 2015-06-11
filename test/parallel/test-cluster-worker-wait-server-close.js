'use strict';

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (cluster.isWorker) {
  net.createServer(function(socket) {
    // Wait for any data, then close connection
    socket.write('.');
    socket.on('data', function discard() {
    });
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
      this.on('data', function() {
        console.log('got data from client');
        // socket definitely connected to worker if we got data
        worker.disconnect();
        setTimeout(function() {
          socket.end();
          connectionDone = true;
        }, 1000);
      });
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
