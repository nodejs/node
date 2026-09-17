'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Worker } = require('node:worker_threads');

// The event loop metrics are tracked per event loop, so they are also
// available in worker threads.
const worker = new Worker(`
  const { parentPort } = require('node:worker_threads');
  const { performance } = require('node:perf_hooks');
  setImmediate(() => {
    const info = performance.nodeTiming.uvMetricsInfo;
    const infoBigInt = performance.nodeTiming.uvMetricsInfoBigInt;
    parentPort.postMessage({ info, infoBigInt });
  });
`, { eval: true });

worker.on('message', common.mustCall(({ info, infoBigInt }) => {
  for (const key of ['loopCount', 'events', 'eventsWaiting']) {
    assert.strictEqual(typeof info[key], 'number');
    assert.strictEqual(typeof infoBigInt[key], 'bigint');
    assert.strictEqual(BigInt(info[key]), infoBigInt[key]);
  }
  assert.ok(infoBigInt.loopCount > 0n);
}));
