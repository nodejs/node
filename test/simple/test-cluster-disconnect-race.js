'use strict';

// This code triggers an AssertionError on Linux in Node.js 5.3.0 and earlier.
// Ref: https://github.com/nodejs/node/issues/4205

var common = require('../common');
var assert = require('assert');
var net = require('net');
var cluster = require('cluster');
cluster.schedulingPolicy = cluster.SCHED_NONE;

if (process.platform === 'win32') {
  console.log('1..0 # Skipped: This test does not apply to Windows.');
  return;
}

if (cluster.isMaster) {
  var worker1, worker2;

  worker1 = cluster.fork();
  worker1.on('message', common.mustCall(function() {
    worker2 = cluster.fork();
    worker1.disconnect();
    worker2.on('online', common.mustCall(worker2.disconnect));
  }, 2));

  cluster.on('exit', function(worker, code) {
    assert.strictEqual(code, 0, 'worker exited with error');
  });

  return;
}

var server = net.createServer();

server.listen(common.PORT, function retry() {
  process.send('listening');
  process.send('listening');
});
