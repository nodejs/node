'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, parentPort } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 4);
    w.terminate();
  }));
  w.postMessage(2);
} else {
  parentPort.onmessage = common.mustCall((message) => {
    parentPort.postMessage(message * 2);
  });
}
