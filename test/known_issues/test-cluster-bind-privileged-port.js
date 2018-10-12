'use strict';
const common = require('../common');

// This test should fail on macOS (10.14) due to an issue with privileged ports.

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (!common.isOSXMojave)
  assert.fail('Code should fail only on macOS Mojave.');


if (cluster.isMaster) {
  cluster.fork().on('exit', common.mustCall((exitCode) => {
    assert.strictEqual(exitCode, 0);
  }));
} else {
  const s = net.createServer(common.mustNotCall());
  s.listen(42, common.mustNotCall('listen should have failed'));
  s.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EACCES');
    process.disconnect();
  }));
}
