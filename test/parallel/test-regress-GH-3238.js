'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  function forkWorker(action) {
    const worker = cluster.fork({ action });
    worker.on('disconnect', common.mustCall(() => {
      assert.strictEqual(worker.exitedAfterDisconnect, true);
    }));

    worker.on('exit', common.mustCall(() => {
      assert.strictEqual(worker.exitedAfterDisconnect, true);
    }));
  }

  forkWorker('disconnect');
  forkWorker('kill');
} else {
  cluster.worker[process.env.action]();
}
