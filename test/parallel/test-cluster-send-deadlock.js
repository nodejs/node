'use strict';
// Testing mutual send of handles: from master to worker, and from worker to
// master.

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (cluster.isMaster) {
  var worker = cluster.fork();
  worker.on('exit', function(code, signal) {
    assert.equal(code, 0, 'Worker exited with an error code');
    assert(!signal, 'Worker exited by a signal');
    server.close();
  });

  var server = net.createServer(function(socket) {
    worker.send('handle', socket);
  });

  server.listen(common.PORT, function() {
    worker.send('listen');
  });
} else {
  process.on('message', function(msg, handle) {
    if (msg === 'listen') {
      var client1 = net.connect({ host: 'localhost', port: common.PORT });
      var client2 = net.connect({ host: 'localhost', port: common.PORT });
      var waiting = 2;
      client1.on('close', onclose);
      client2.on('close', onclose);
      function onclose() {
        if (--waiting === 0)
          cluster.worker.disconnect();
      }
      setTimeout(function() {
        client1.end();
        client2.end();
      }, 50);
    } else {
      process.send('reply', handle);
    }
  });
}
