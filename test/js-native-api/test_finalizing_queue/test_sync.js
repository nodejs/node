'use strict';
// Flags: --expose-gc

// The test verifies that the finalizing queue is processed synchronously
// when we call a method that may cause a GC call.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

assert.strictEqual(test.finalizeCount, 0);
async function runGCTests() {
  (() => {
    test.createObject();
  })();
  global.gc();
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 1);

  (() => {
    test.createObject();
    test.createObject();
  })();
  global.gc();
  test.drainFinalizingQueue();
  assert.strictEqual(test.finalizeCount, 3);
}
runGCTests();
