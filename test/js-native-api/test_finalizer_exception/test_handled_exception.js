'use strict';
// Flags: --expose-gc

// The test verifies that if finalizer throws an exception,
// then it can be handled by custom finalizer error handler.
// If it is not handled in the handler, causing a
// Node.js uncaughtException.

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

  let handledExceptions = 0;
  test.setFinalizerErrorHandler((err) => {
    ++handledExceptions;
    assert.strictEqual(err.message, 'Error during Finalize');
    if (handledExceptions == 2) {
      // One time we report the error to be unhandled
      return false;
    }
  });

  (() => {
    test.createObject(true /* throw on destruct */);
  })();
  gcUntilSync(1);
  assert.strictEqual(test.finalizeCount, 1);
  assert.strictEqual(handledExceptions, 1);
  assert.strictEqual(unhandledExceptions, 0);

  (() => {
    test.createObject(true /* throw on destruct */);
    test.createObject(true /* throw on destruct */);
  })();
  gcUntilSync(3);
  assert.strictEqual(test.finalizeCount, 3);
  assert.strictEqual(handledExceptions, 3);
  assert.strictEqual(unhandledExceptions, 1);

  test.setFinalizerErrorHandler(null);
  (() => {
    test.createObject(true /* throw on destruct */);
  })();
  gcUntilSync(4);
  assert.strictEqual(test.finalizeCount, 4);
  assert.strictEqual(handledExceptions, 3);
  assert.strictEqual(unhandledExceptions, 2);

  // Raise unhandled exception if finalizer error handler throws.
  test.setFinalizerErrorHandler((err) => {
    ++handledExceptions;
    assert.strictEqual(err.message, 'Error during Finalize');
    throw err;
  });
  (() => {
    test.createObject(true /* throw on destruct */);
  })();
  gcUntilSync(5);
  assert.strictEqual(test.finalizeCount, 5);
  assert.strictEqual(handledExceptions, 4);
  assert.strictEqual(unhandledExceptions, 3);
}
runGCTests();
