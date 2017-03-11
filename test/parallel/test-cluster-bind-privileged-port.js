'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (common.isWindows) {
  common.skip('not reliable on Windows.');
  return;
}

if (process.getuid() === 0) {
  common.skip('Test is not supposed to be run as root.');
  return;
}

if (cluster.isMaster) {
  cluster.fork().on('exit', common.mustCall(function(exitCode) {
    assert.equal(exitCode, 0);
  }));
} else {
  const s = net.createServer(common.fail);
  s.listen(42, common.fail.bind(null, 'listen should have failed'));
  s.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'EACCES');
    process.disconnect();
  }));
}
