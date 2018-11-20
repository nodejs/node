'use strict';
const common = require('../common');

// Check that cluster works perfectly for both `kill` and `disconnect` cases.
// Also take into account that the `disconnect` event may be received after the
// `exit` event.
// https://github.com/nodejs/node/issues/3238

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
