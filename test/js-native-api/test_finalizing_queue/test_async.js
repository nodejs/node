'use strict';
// Flags: --expose-gc

// The test verifies that the finalizing queue is processed asynchronously in
// setImmediate when we do not call methods that could cause the GC calls.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

assert.strictEqual(test.finalizeCount, 0);
async function runGCTests() {
  (() => {
    test.createObject();
  })();
  global.gc();
  assert.strictEqual(test.finalizeCount, 0);
  await common.gcUntil('test 1', () => (test.finalizeCount === 1));

  (() => {
    test.createObject();
    test.createObject();
  })();
  global.gc();
  assert.strictEqual(test.finalizeCount, 1);
  await common.gcUntil('test 2', () => (test.finalizeCount === 3));
}
runGCTests();
