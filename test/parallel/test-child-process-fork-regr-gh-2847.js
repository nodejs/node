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
}).listen(0, function() {
  var worker = cluster.fork();

  function send(callback) {
    var s = net.connect(server.address().port, function() {
      worker.send({}, s, callback);
    });

    // Errors can happen if this connection
    // is still happening while the server has been closed.
    s.on('error', function(err) {
      if ((err.code !== 'ECONNRESET') &&
          ((err.code !== 'ECONNREFUSED'))) {
        throw err;
      }
    });
  }

  worker.process.once('close', common.mustCall(function() {
    // Otherwise the crash on `_channel.fd` access may happen
    assert(worker.process._channel === null);
    server.close();
  }));

  send();
  send(function(err) {
    // Ignore errors when sending the second handle because the worker
    // may already have exited.
    if (err) {
      if (err.code !== 'ECONNREFUSED') {
        throw err;
      }
    }
  });
});
