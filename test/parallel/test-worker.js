'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

const kTestString = 'Hello, world!';

if (isMainThread) {
  const w = new Worker(__filename);
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, kTestString);
  }));
} else {
  setImmediate(() => {
    process.nextTick(() => {
      parentPort.postMessage(kTestString);
    });
  });
}
