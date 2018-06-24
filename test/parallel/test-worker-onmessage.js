// Flags: --experimental-worker
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
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
