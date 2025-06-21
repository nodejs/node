'use strict';
const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');

common.skipIfInspectorDisabled();

checkForInspectSupport('--inspect');

function checkForInspectSupport(flag) {

  const nodeOptions = JSON.stringify(flag);
  const numWorkers = 2;
  process.env.NODE_OPTIONS = flag;

  if (cluster.isPrimary) {
    for (let i = 0; i < numWorkers; i++) {
      cluster.fork();
    }

    cluster.on('online', (worker) => {
      worker.disconnect();
    });

    cluster.on('exit', common.mustCall((worker, code, signal) => {
      const errMsg = `For NODE_OPTIONS ${nodeOptions}, failed to start cluster`;
      assert.strictEqual(worker.exitedAfterDisconnect, true, errMsg);
    }, numWorkers));
  }
}
