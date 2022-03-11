'use strict';
// Flags: --expose-gc

// The test verifies that the finalizing queue is processed asynchronously in
// setImmediate when we do not call methods that could cause the GC calls.
// If a finalizer throws a JS exception, then it is causing a Node.js
// uncaughtException, and the queue processing is stopped.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

async function runGCTests() {
  let exceptionCount = 0;
  process.on('uncaughtException', (err) => {
    ++exceptionCount;
    assert.strictEqual(err.message, 'Error during Finalize');
  });

  assert.strictEqual(test.finalizeCount, 0);
  (() => {
    test.createObject(/* throw on destruct */ true);
  })();
  await common.gcUntil('test 1', () => (test.finalizeCount === 1));
  assert.strictEqual(exceptionCount, 1);

  (() => {
    test.createObject(/* throw on destruct */ true);
    test.createObject(/* throw on destruct */ true);
  })();
  // All errors are handled separately
  await common.gcUntil('test 2', () => (test.finalizeCount === 3));
  assert.strictEqual(exceptionCount, 3);
}
runGCTests();
