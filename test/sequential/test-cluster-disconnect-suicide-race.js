'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const os = require('os');

if (cluster.isMaster) {
  function forkWorker(action) {
    const worker = cluster.fork({ action });
    worker.on('disconnect', common.mustCall(() => {
      assert.strictEqual(worker.suicide, true);
    }));

    worker.on('exit', common.mustCall(() => {
      assert.strictEqual(worker.suicide, true);
    }));
  }

  const cpus = os.cpus().length;
  const tries = cpus > 8 ? 64 : cpus * 8;

  cluster.on('exit', common.mustCall((worker, code) => {
    assert.strictEqual(code, 0, 'worker exited with error');
  }, tries * 2));

  for (let i = 0; i < tries; ++i) {
    forkWorker('disconnect');
    forkWorker('kill');
  }
} else {
  cluster.worker[process.env.action]();
}
