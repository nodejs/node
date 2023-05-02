'use strict';

// Flags: --harmony-struct

const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
  let m = new globalThis.SharedArray(16);

  let worker = new Worker(__filename);
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
