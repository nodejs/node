'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (process.platform === 'win32') {
  console.log('Skipping test, not reliable on Windows.');
  process.exit(0);
}

if (process.getuid() === 0) {
  console.log('Do not run this test as root.');
  process.exit(0);
}

if (cluster.isMaster) {
  // Master opens and binds the socket and shares it with the worker.
  cluster.schedulingPolicy = cluster.SCHED_NONE;
  cluster.fork().on('exit', common.mustCall(function(exitCode) {
    assert.equal(exitCode, 0);
  }));
}
else {
  var s = net.createServer(assert.fail);
  s.listen(42, assert.fail.bind(null, 'listen should have failed'));
  s.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'EACCES');
    process.disconnect();
  }));
}
