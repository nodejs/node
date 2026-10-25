'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Worker } = require('node:worker_threads');

const worker = new Worker('', { eval: true });
const listener = () => {};

worker.on('message', listener);
worker.on('messageerror', listener);

worker.on('exit', common.mustCall(() => {
  assert.strictEqual(worker.listenerCount('message'), 0);
  assert.strictEqual(worker.listenerCount('messageerror'), 0);
}));
