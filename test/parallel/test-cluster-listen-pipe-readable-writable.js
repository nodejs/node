'use strict';
const common = require('../common');

if (common.isWindows) {
  common.skip('skip on Windows');
  return;
}

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');
const fs = require('fs');

if (cluster.isPrimary) {
  cluster.fork();
} else {
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const server = net.createServer().listen({
    path: common.PIPE,
    readableAll: true,
    writableAll: true,
  }, common.mustCall(() => {
    const stat = fs.statSync(common.PIPE);
    assert.strictEqual(stat.mode & 0o777, 0o777);
    server.close();
    process.disconnect();
  }));
}
