'use strict';

// Test should fail in Node.js 5.4.1 and pass in later versions.

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  cluster.on('exit', (worker, code) => {
    assert.strictEqual(code, 0, 'worker exited with error');
  });

  return cluster.fork();
}

let eventFired = false;

cluster.worker.disconnect();

process.nextTick(common.mustCall(() => {
  assert.strictEqual(eventFired, false, 'disconnect event should wait for ack');
}));

cluster.worker.on('disconnect', common.mustCall(() => {
  eventFired = true;
}));
