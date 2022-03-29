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
  let unhandledExceptions = 0;
  process.on('uncaughtException', (err) => {
    ++unhandledExceptions;
    assert.strictEqual(err.message, 'Error during Finalize');
  });

  (() => {
    test.createObject(true /* throw on destruct */);
  })();
  gcUntilSync(1);
  assert.strictEqual(test.finalizeCount, 1);
  assert.strictEqual(unhandledExceptions, 1);

  (() => {
    test.createObject(true /* throw on destruct */);
    test.createObject(true /* throw on destruct */);
  })();
  gcUntilSync(3);
  assert.strictEqual(test.finalizeCount, 3);
  assert.strictEqual(unhandledExceptions, 3);
}
runGCTests();
