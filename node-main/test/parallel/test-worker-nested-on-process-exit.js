'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, workerData } = require('worker_threads');

// Test that 'exit' events for nested Workers are not received when a Worker
// terminates itself through process.exit().

if (workerData === null) {
  const nestedWorkerExitCounter = new Int32Array(new SharedArrayBuffer(4));
  const w = new Worker(__filename, { workerData: nestedWorkerExitCounter });
  w.on('exit', common.mustCall(() => {
    assert.strictEqual(nestedWorkerExitCounter[0], 0);
  }));
} else {
  const nestedWorker = new Worker('setInterval(() => {}, 100)', { eval: true });
  // The counter should never be increased here.
  nestedWorker.on('exit', () => workerData[0]++);
  nestedWorker.on('online', () => process.exit());
}
