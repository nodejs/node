'use strict';

const common = require('../common');

const assert = require('assert');
const { Worker, isMainThread, threadName } = require('worker_threads');

const name = 'test-worker-thread-name';

if (isMainThread) {
  assert.strictEqual(threadName, '');
  const w = new Worker(__filename, { name });
  assert.strictEqual(w.threadName, name);
  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(w.threadName, '');
  }));
} else {
  assert.strictEqual(threadName, name);
  process.exit(0);
}
