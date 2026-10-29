'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { Worker } = require('worker_threads');

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  const { createHistogram } = require('perf_hooks');

  const histogram = createHistogram({ highest: 200000, figures: 5 });
  for (let i = 1; i <= 100000; i++) histogram.record(i);
  histogram.qrde({ bins: 1000, dequantize: 'all' });
  parentPort.postMessage('scheduled');
`, { eval: true });

(async () => {
  assert.deepStrictEqual(await once(worker, 'message'), ['scheduled']);
  assert.strictEqual(await worker.terminate(), 1);
})().then(common.mustCall());
