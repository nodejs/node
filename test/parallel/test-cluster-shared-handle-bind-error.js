'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (cluster.isMaster) {
  // Master opens and binds the socket and shares it with the worker.
  cluster.schedulingPolicy = cluster.SCHED_NONE;
  // Hog the TCP port so that when the worker tries to bind, it'll fail.
  net.createServer(common.fail).listen(common.PORT, function() {
    var server = this;
    var worker = cluster.fork();
    worker.on('exit', common.mustCall(function(exitCode) {
      assert.equal(exitCode, 0);
      server.close();
    }));
  });
} else {
  var s = net.createServer(common.fail);
  s.listen(common.PORT, common.fail.bind(null, 'listen should have failed'));
  s.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'EADDRINUSE');
    process.disconnect();
  }));
}
