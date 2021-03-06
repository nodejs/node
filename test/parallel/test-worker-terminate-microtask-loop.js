'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Verify that `.terminate()` interrupts the microtask queue.

const worker = new Worker(`
function loop() { Promise.resolve().then(loop); } loop();
require('worker_threads').parentPort.postMessage('up');
`, { eval: true });

worker.once('message', common.mustCall(() => {
  setImmediate(() => worker.terminate());
}));

worker.once('exit', common.mustCall((code) => {
  assert.strictEqual(code, 1);
}));
