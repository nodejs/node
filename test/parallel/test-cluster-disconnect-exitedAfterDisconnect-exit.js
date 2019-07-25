'use strict';

const common = require('../common');

// Test should fail in Node.js 5.4.1 and pass in later versions.

const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  cluster.on('exit', common.mustCall((worker, code) => {
    assert.strictEqual(code, 0, `worker exited with code: ${code}, expected 0`);
    assert.strictEqual(worker.exitedAfterDisconnect, false);
  }));

  return cluster.fork();
}

process.exit(0);
