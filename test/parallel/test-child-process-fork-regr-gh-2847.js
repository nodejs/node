'use strict';

const common = require('../common');
const assert = require('assert');

const cluster = require('cluster');
const net = require('net');

if (!cluster.isMaster) {
  // Exit on first received handle to leave the queue non-empty in master
  process.on('message', function() {
    process.exit(1);
  });
  return;
}

var server = net.createServer(function(s) {
  if (common.isWindows) {
    s.on('error', function(err) {
      // Prevent possible ECONNRESET errors from popping up
      if (err.code !== 'ECONNRESET')
        throw err;
    });
  }
  setTimeout(function() {
    s.destroy();
  }, 100);
}).listen(common.PORT, function() {
  var worker = cluster.fork();

  function send(callback) {
    var s = net.connect(common.PORT, function() {
      worker.send({}, s, callback);
    });
  }

  worker.process.once('close', common.mustCall(function() {
    // Otherwise the crash on `_channel.fd` access may happen
    assert(worker.process._channel === null);
    server.close();
  }));

  // Send 2 handles to make `process.disconnect()` wait
  send();
  send();
});
