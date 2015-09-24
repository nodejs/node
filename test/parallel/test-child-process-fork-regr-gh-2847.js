'use strict';

const common = require('../common');
const assert = require('assert');

const cluster = require('cluster');
const net = require('net');
const util = require('util');

if (!cluster.isMaster) {
  // Exit on first received handle to leave the queue non-empty in master
  process.on('message', function() {
    process.exit(1);
  });
  return;
}

var server = net.createServer(function(s) {
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

  // Queue up several handles, to make `process.disconnect()` wait
  for (var i = 0; i < 100; i++)
    send();
});
