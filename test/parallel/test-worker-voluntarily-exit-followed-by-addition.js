'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  const workerData = new Int32Array(new SharedArrayBuffer(4));
  new Worker(__filename, {
    workerData,
  });
  process.on('beforeExit', common.mustCall(() => {
    assert.strictEqual(workerData[0], 0);
  }));
} else {
  const { workerData } = require('worker_threads');
  process.exit();
  workerData[0] = 1;
}
