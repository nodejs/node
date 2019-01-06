'use strict';
const assert = require('assert');
const common = require('../common');
const { isMainThread, parentPort, Worker } = require('worker_threads');

// This test makes sure that we manipulate the references of
// `parentPort` correctly so that any worker threads will
// automatically exit when there are no any other references.
{
  if (isMainThread) {
    const worker = new Worker(__filename);

    worker.on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0);
    }), 1);

    worker.on('online', common.mustCall());
  } else {
    const messageCallback = () => {};
    parentPort.on('message', messageCallback);
    // The thread won't exit if we don't make the 'message' listener off.
    parentPort.off('message', messageCallback);
  }
}
