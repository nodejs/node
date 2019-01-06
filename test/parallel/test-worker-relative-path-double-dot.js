'use strict';
const path = require('path');
const assert = require('assert');
const common = require('../common');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (isMainThread) {
  const cwdName = path.relative('../', '.');
  const relativePath = path.relative('.', __filename);
  const w = new Worker(path.join('..', cwdName, relativePath));
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'Hello, world!');
  }));
} else {
  parentPort.postMessage('Hello, world!');
}
