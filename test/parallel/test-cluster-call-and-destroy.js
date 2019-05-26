'use strict';
const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');

if (cluster.isMaster) {
  const worker = cluster.fork();
  worker.on('disconnect', common.mustCall(() => {
    assert.strictEqual(worker.isConnected(), false);
    worker.destroy();
  }));
} else {
  assert.strictEqual(cluster.worker.isConnected(), true);
  cluster.worker.disconnect();
}
