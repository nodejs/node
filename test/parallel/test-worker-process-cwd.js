'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

// Do not use isMainThread directly, otherwise the test would time out in case
// it's started inside of another worker thread.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = '1';
  if (!isMainThread) {
    common.skip('This test can only run as main thread');
  }
  const w = new Worker(__filename);
  process.chdir('..');
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, process.cwd());
    process.chdir('..');
    w.postMessage(process.cwd());
  }));
} else if (!process.env.SECOND_WORKER) {
  process.env.SECOND_WORKER = '1';
  const firstCwd = process.cwd();
  const w = new Worker(__filename);
  w.on('message', common.mustCall((message) => {
    assert.strictEqual(message, process.cwd());
    parentPort.postMessage(firstCwd);
    parentPort.onmessage = common.mustCall((obj) => {
      const secondCwd = process.cwd();
      assert.strictEqual(secondCwd, obj.data);
      assert.notStrictEqual(secondCwd, firstCwd);
      w.postMessage(secondCwd);
      parentPort.unref();
    });
  }));
} else {
  const firstCwd = process.cwd();
  parentPort.postMessage(firstCwd);
  parentPort.onmessage = common.mustCall((obj) => {
    const secondCwd = process.cwd();
    assert.strictEqual(secondCwd, obj.data);
    assert.notStrictEqual(secondCwd, firstCwd);
    process.exit();
  });
}
