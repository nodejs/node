'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
  const w = new Worker(`./${path.relative('.', __filename)}`);
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'Hello, world!');
  }));
} else {
  parentPort.postMessage('Hello, world!');
}
