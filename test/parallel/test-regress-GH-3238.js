'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  const worker = cluster.fork();
  let disconnected = false;

  worker.on('disconnect', common.mustCall(function() {
    assert.strictEqual(worker.suicide, true);
    disconnected = true;
  }));

  worker.on('exit', common.mustCall(function() {
    assert.strictEqual(worker.suicide, true);
    assert.strictEqual(disconnected, true);
  }));
} else {
  cluster.worker.disconnect();
}
