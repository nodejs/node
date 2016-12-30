'use strict';

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

var serverClosed = false;

if (cluster.isWorker) {
  var server = net.createServer(function(socket) {
    // Wait for any data, then close connection
    socket.write('.');
    socket.on('data', function discard() {});
  }).listen(common.PORT, common.localhostIPv4);

  server.once('close', function() {
    serverClosed = true;
  });

  // Although not typical, the worker process can exit before the disconnect
  // event fires. Use this to keep the process open until the event has fired.
  var keepOpen = setInterval(function() {}, 9999);

  // Check worker events and properties
  process.once('disconnect', function() {
    // disconnect should occur after socket close
    assert(serverClosed);
    clearInterval(keepOpen);
  });
} else if (cluster.isMaster) {
  // start worker
  var worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', function() {
    const socket = net.createConnection(common.PORT, common.localhostIPv4);

    socket.on('connect', function() {
      socket.on('data', function() {
        console.log('got data from client');
        // socket definitely connected to worker if we got data
        worker.disconnect();
        socket.end();
      });
    });
  });
}
