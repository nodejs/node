'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
  const w = new Worker(__filename);
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'Hello, world!');
  }));
} else {
  setImmediate(() => {
    process.nextTick(() => {
      parentPort.postMessage('Hello, world!');
    });
  });
}
