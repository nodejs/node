'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

// Verify that cwd changes from the main thread are handled correctly in
// workers.

// Do not use isMainThread directly, otherwise the test would time out in case
// it's started inside of another worker thread.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = '1';
  if (!isMainThread) {
    common.skip('This test can only run as main thread');
  }
  // Normalize the current working dir to also work with the root folder.
  process.chdir(__dirname);

  assert(!process.cwd.toString().includes('Atomics.load'));

  // 1. Start the first worker.
  const w = new Worker(__filename);
  w.once('message', common.mustCall((message) => {
    // 5. Change the cwd and send that to the spawned worker.
    assert.strictEqual(message, process.cwd());
    process.chdir('..');
    w.postMessage(process.cwd());
  }));
} else if (!process.env.SECOND_WORKER) {
  process.env.SECOND_WORKER = '1';

  // 2. Save the current cwd and verify that `process.cwd` includes the
  //    Atomics.load call and spawn a new worker.
  const cwd = process.cwd();
  assert(process.cwd.toString().includes('Atomics.load'));

  const w = new Worker(__filename);
  w.once('message', common.mustCall((message) => {
    // 4. Verify at the current cwd is identical to the received and the
    //    original one.
    assert.strictEqual(process.cwd(), message);
    assert.strictEqual(message, cwd);
    parentPort.postMessage(cwd);
  }));

  parentPort.once('message', common.mustCall((message) => {
    // 6. Verify that the current cwd is identical to the received one but not
    //    with the original one.
    assert.strictEqual(process.cwd(), message);
    assert.notStrictEqual(message, cwd);
    w.postMessage(message);
  }));
} else {
  // 3. Save the current cwd, post back to the "main" thread and verify that
  //    `process.cwd` includes the Atomics.load call.
  const cwd = process.cwd();
  // Send the current cwd to the parent.
  parentPort.postMessage(cwd);
  assert(process.cwd.toString().includes('Atomics.load'));

  parentPort.once('message', common.mustCall((message) => {
    // 7. Verify that the current cwd is identical to the received one but
    //    not with the original one.
    assert.strictEqual(process.cwd(), message);
    assert.notStrictEqual(message, cwd);
  }));
}
