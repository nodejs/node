'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker, threadName, workerData } = require('worker_threads');

const name = 'test-worker-thread-name';

if (workerData?.isWorker) {
  assert.strictEqual(threadName, name);
  process.exit(0);
} else {
  const w = new Worker(__filename, { name, workerData: { isWorker: true } });
  assert.strictEqual(w.threadName, name);
  w.on('exit', common.mustCall(() => {
    assert.strictEqual(w.threadName, null);
  }));
}
