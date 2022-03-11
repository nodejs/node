'use strict';
// Flags: --expose-gc

// The test verifies that the finalizing queue is processed synchronously
// when we call a method that may cause a GC call.
// This method throws a JS exception when a finalizer from the queue
// throws a JS exception.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizing_queue`);

assert.strictEqual(test.finalizeCount, 0);
async function runGCTests() {
  (() => {
    test.createObject(/* throw on destruct */ true);
  })();
  global.gc();
  assert.throws(() => test.drainFinalizingQueue(), {
    name: 'Error',
    message: 'Error during Finalize'
  });
  assert.strictEqual(test.finalizeCount, 1);

  (() => {
    test.createObject(/* throw on destruct */ true);
    test.createObject(/* throw on destruct */ true);
  })();
  global.gc();
  // The finalizing queue processing is stopped on the first JS exception.
  assert.throws(() => test.drainFinalizingQueue(), {
    name: 'Error',
    message: 'Error during Finalize'
  });
  assert.strictEqual(test.finalizeCount, 2);
  // To handle the next items in the the finalizing queue we
  // must call a GC-touching function again.
  assert.throws(() => test.drainFinalizingQueue(), {
    name: 'Error',
    message: 'Error during Finalize'
  });
  assert.strictEqual(test.finalizeCount, 3);
}
runGCTests();
