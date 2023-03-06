'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

common.skipIfInspectorDisabled();
common.skipIfWorker(); // This test requires both main and worker threads.

const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  const name = 'Hello Thread';
  const expectedTitle = `[worker 1] ${name}`;
  const worker = new Worker(fixtures.path('worker-name.js'), {
    name,
  });
  worker.once('message', common.mustCall((message) => {
    assert.strictEqual(message, expectedTitle);
    worker.postMessage('done');
  }));
}
