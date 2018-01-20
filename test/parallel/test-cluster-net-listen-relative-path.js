'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');
const path = require('path');
const fs = require('fs');

if (common.isWindows)
  common.skip('On Windows named pipes live in their own ' +
              'filesystem and don\'t have a ~100 byte limit');

// Choose a socket name such that the absolute path would exceed 100 bytes.
const socketDir = './unix-socket-dir';
const socketName = 'A'.repeat(100 - socketDir.length - 1);

// Make sure we're not in a weird environment
assert.strictEqual(path.resolve(socketDir, socketName).length > 100, true,
                   'absolute socket path should be longer than 100 bytes');

if (cluster.isMaster) {
  // ensure that the worker exits peacefully
  common.refreshTmpDir();
  process.chdir(common.tmpDir);
  fs.mkdirSync(socketDir);
  cluster.fork().on('exit', common.mustCall(function(statusCode) {
    assert.strictEqual(statusCode, 0);

    assert.strictEqual(
      fs.existsSync(path.join(socketDir, socketName)), false,
      'Socket should be removed when the worker exits');
  }));
} else {
  process.chdir(socketDir);

  const server = net.createServer(common.mustNotCall());

  server.listen(socketName, common.mustCall(function() {
    assert.strictEqual(
      fs.existsSync(socketName), true,
      'Socket created in CWD');

    process.disconnect();
  }));
}
