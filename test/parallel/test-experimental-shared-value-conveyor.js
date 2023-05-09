'use strict';

// Flags: --harmony-struct

const common = require('../common');
const assert = require('assert');
const { Worker, parentPort } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const m = new globalThis.SharedArray(16);

  const worker = new Worker(__filename);
  worker.once('message', common.mustCall((message) => {
    assert.strictEqual(message, m);
  }));

  worker.postMessage(m);
} else {
  parentPort.once('message', common.mustCall((message) => {
    // Simple echo.
    parentPort.postMessage(message);
  }));
}
