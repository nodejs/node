'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('not reliable on Windows');

if (process.getuid() === 0)
  common.skip('as this test should not be run as `root`');

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  // Master opens and binds the socket and shares it with the worker.
  cluster.schedulingPolicy = cluster.SCHED_NONE;
  cluster.fork().on('exit', common.mustCall(function(exitCode) {
    assert.strictEqual(exitCode, 0);
  }));
} else {
  const s = net.createServer(common.mustNotCall());
  s.listen(42, common.mustNotCall('listen should have failed'));
  s.on('error', common.mustCall(function(err) {
    assert.strictEqual(err.code, 'EACCES');
    process.disconnect();
  }));
}
