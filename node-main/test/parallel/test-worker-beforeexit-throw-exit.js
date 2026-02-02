'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Test that 'exit' is emitted if 'beforeExit' throws, both inside the Worker.

const workerData = new Uint8Array(new SharedArrayBuffer(2));
const w = new Worker(`
  const { workerData } = require('worker_threads');
  process.on('exit', () => {
    workerData[0] = 100;
  });
  process.on('beforeExit', () => {
    workerData[1] = 200;
    throw new Error('banana');
  });
`, { eval: true, workerData });

w.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'banana');
}));

w.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 1);
  assert.strictEqual(workerData[0], 100);
  assert.strictEqual(workerData[1], 200);
}));
