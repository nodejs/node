'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const w = new Worker(`
require('worker_threads').parentPort.postMessage(performance.timeOrigin);
`, { eval: true });

w.on('message', common.mustCall((timeOrigin) => {
  // Worker is created after this main context, it's
  // `performance.timeOrigin` must be greater than this
  // main context's.
  assert.ok(timeOrigin > performance.timeOrigin);
}));

w.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
