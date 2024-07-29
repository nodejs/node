'use strict';

const common = require('../common');
const { Worker, workerData } = require('worker_threads');
const assert = require('assert');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);
  const w = new Worker(__filename, { workerData: i32a });
  const chunks = [];
  w.stdout.on('data', (chunk) => chunks.push(chunk));
  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(Buffer.concat(chunks).toString().trim(), 'timed-out');
    assert.strictEqual(code, 0);
  }));
} else {
  const i32a = workerData;
  const result = Atomics.waitAsync(i32a, 0, 0, 1000);
  result.value.then((val) => console.log(val));
}
