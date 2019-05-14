'use strict';
const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');

if (cluster.isMaster) {
  const worker = cluster.fork();

  worker.on('online', common.mustCall(() => {
    // Use worker.process.kill() instead of worker.kill() because the latter
    // waits for a graceful disconnect, which will never happen.
    worker.process.kill();
  }));

  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(signal, 'SIGTERM');
  }));
} else {
  while (true) {}
}
