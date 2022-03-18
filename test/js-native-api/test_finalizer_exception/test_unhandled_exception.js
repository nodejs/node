'use strict';
// Flags: --expose-gc

// The test verifies that if finalizer throws an exception,
// then it is causing a Node.js uncaughtException.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizer_exception`);

assert.strictEqual(test.finalizeCount, 0);

function gcUntilSync(value) {
  for (let i = 0; i < 10; ++i) {
    global.gc();
    if (test.finalizeCount === value) {
      break;
    }
  }
}

function runGCTests() {
  let exceptionCount = 0;
  process.on('uncaughtException', (err) => {
    ++exceptionCount;
    assert.strictEqual(err.message, 'Error during Finalize');
  });

  (() => {
    test.createObject(/* throw on destruct */ true);
  })();
  gcUntilSync(1);
  assert.strictEqual(test.finalizeCount, 1);

  (() => {
    test.createObject(/* throw on destruct */ true);
    test.createObject(/* throw on destruct */ true);
  })();
  gcUntilSync(3);
  assert.strictEqual(test.finalizeCount, 3);
}
runGCTests();
