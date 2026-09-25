'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, parentPort } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);
  const expectation = [ 4, undefined, null ];
  const actual = [];
  w.on('message', common.mustCall((message) => {
    actual.push(message);
    if (actual.length === expectation.length) {
      assert.deepStrictEqual(expectation, actual);
      w.terminate();
    }
  }, expectation.length));
  w.postMessage(2);
} else {
  parentPort.onmessage = common.mustCall((message) => {
    parentPort.postMessage(message.data * 2);
    parentPort.postMessage(undefined);
    parentPort.postMessage(null);
  });
}
