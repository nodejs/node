'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const w = new Worker(`
require('worker_threads').parentPort.postMessage(performance.timeOrigin);
`, { eval: true });

w.on('message', common.mustCall((timeOrigin) => {
  // PerformanceNodeTiming exposes process milestones so the
  // `performance.timeOrigin` in the `worker_threads.Worker` must be the start
  // time of the process.
  assert.strictEqual(timeOrigin, performance.timeOrigin);
}));

w.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
