'use strict';
const common = require('../common');
const assert = require('assert');
const { isMainThread, Worker } = require('worker_threads');

// Check that `process.exit()` can be called inside a Worker from an uncaught
// exception handler.

if (isMainThread) {
  const w = new Worker(__filename);
  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 42);
  }));
  return;
}

process.on('uncaughtException', () => {
  process.exit(42);
});

throw new Error();
