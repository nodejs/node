'use strict';
// test-cluster-worker-init.js
// verifies that, when a child process is forked, the cluster.worker
// object can receive messages as expected

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const msg = 'foo';

if (cluster.isMaster) {
  const worker = cluster.fork();

  worker.on('message', common.mustCall((message) => {
    assert.strictEqual(message, true, 'did not receive expected message');
    const w = worker.disconnect();
    assert.strictEqual(worker, w, 'did not return a reference');
  }));

  worker.on('online', () => {
    worker.send(msg);
  });
} else {
  // GH #7998
  cluster.worker.on('message', (message) => {
    process.send(message === msg);
  });
}
