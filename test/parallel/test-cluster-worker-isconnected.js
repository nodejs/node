'use strict';
const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');

if (cluster.isMaster) {
  const worker = cluster.fork();

  assert.strictEqual(worker.isConnected(), true);

  worker.on('disconnect', common.mustCall(() => {
    assert.strictEqual(worker.isConnected(), false);
  }));

  worker.on('message', function(msg) {
    if (msg === 'readyToDisconnect') {
      worker.disconnect();
    }
  });
} else {
  function assertNotConnected() {
    assert.strictEqual(cluster.worker.isConnected(), false);
  }

  assert.strictEqual(cluster.worker.isConnected(), true);
  cluster.worker.on('disconnect', common.mustCall(assertNotConnected));
  cluster.worker.process.on('disconnect', common.mustCall(assertNotConnected));

  process.send('readyToDisconnect');
}
