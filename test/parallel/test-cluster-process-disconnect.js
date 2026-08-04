'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isPrimary) {
  const worker = cluster.fork();
  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(
      code,
      0,
      `Worker did not exit normally with code: ${code}`
    );
    assert.strictEqual(
      signal,
      null,
      `Worker did not exit normally with signal: ${signal}`
    );
  }));
} else {
  const net = require('net');
  const server = net.createServer();
  server.listen(0, common.mustCall(() => {
    process.disconnect();
  }));
}
