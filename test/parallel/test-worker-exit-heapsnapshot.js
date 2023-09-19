'use strict';
const common = require('../common');
const assert = require('assert');
const { getHeapSnapshot } = require('v8');
const { isMainThread, Worker } = require('worker_threads');

// Checks taking heap snapshot at the exit event listener of Worker doesn't
// crash the process.
// Regression for https://github.com/nodejs/node/issues/43122.
if (isMainThread) {
  const worker = new Worker(__filename);

  worker.once('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    getHeapSnapshot().pipe(process.stdout);
  }));
}
