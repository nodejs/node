'use strict';

// This code triggers an AssertionError on Linux in Node.js 5.3.0 and earlier.
// Ref: https://github.com/nodejs/node/issues/4205

const common = require('../common');
if (common.isWindows)
  common.skip('This test does not apply to Windows.');

const assert = require('assert');
const net = require('net');
const cluster = require('cluster');

cluster.schedulingPolicy = cluster.SCHED_NONE;

if (cluster.isMaster) {
  let worker2;

  const worker1 = cluster.fork();
  worker1.on('message', common.mustCall(function() {
    worker2 = cluster.fork();
    worker1.disconnect();
    worker2.on('online', common.mustCall(worker2.disconnect));
  }));

  cluster.on('exit', common.mustCall(function(worker, code) {
    assert.strictEqual(code, 0, `worker exited with error code ${code}`);
  }, 2));

  return;
}

const server = net.createServer();

server.listen(0, function() {
  process.send('listening');
});
