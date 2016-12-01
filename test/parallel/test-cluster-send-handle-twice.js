'use strict';
// Testing to send an handle twice to the parent process.

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

var workers = {
  toStart: 1
};

if (cluster.isMaster) {
  for (var i = 0; i < workers.toStart; ++i) {
    var worker = cluster.fork();
    worker.on('exit', function(code, signal) {
      assert.strictEqual(code, 0, 'Worker exited with an error code');
      assert(!signal, 'Worker exited by a signal');
    });
  }
} else {
  var server = net.createServer(function(socket) {
    process.send('send-handle-1', socket);
    process.send('send-handle-2', socket);
  });

  server.listen(common.PORT, function() {
    var client = net.connect({ host: 'localhost', port: common.PORT });
    client.on('close', function() { cluster.worker.disconnect(); });
    setTimeout(function() { client.end(); }, 50);
  }).on('error', function(e) {
    console.error(e);
    assert(false, 'server.listen failed');
    cluster.worker.disconnect();
  });
}
