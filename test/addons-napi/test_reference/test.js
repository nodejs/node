'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');

const test_reference = require(`./build/${common.buildType}/test_reference`);

// This test script uses external values with finalizer callbacks
// in order to track when values get garbage-collected. Each invocation
// of a finalizer callback increments the finalizeCount property.
assert.strictEqual(test_reference.finalizeCount, 0);

// Run each test function in sequence,
// with an async delay and GC call between each.
function runTests(i, title, tests) {
  if (tests[i]) {
    if (typeof tests[i] === 'string') {
      title = tests[i];
      runTests(i + 1, title, tests);
    } else {
      try {
        tests[i]();
      } catch (e) {
        console.error(`Test failed: ${title}`);
        throw e;
      }
      setImmediate(() => {
        global.gc();
        runTests(i + 1, title, tests);
      });
    }
  }
}
runTests(0, undefined, [

  'External value without a finalizer',
  () => {
    const value = test_reference.createExternal();
    assert.strictEqual(test_reference.finalizeCount, 0);
    assert.strictEqual(typeof value, 'object');
    test_reference.checkExternal(value);
  },
  () => {
    assert.strictEqual(test_reference.finalizeCount, 0);
  },

  'External value with a finalizer',
  () => {
    const value = test_reference.createExternalWithFinalize();
    assert.strictEqual(test_reference.finalizeCount, 0);
    assert.strictEqual(typeof value, 'object');
    test_reference.checkExternal(value);
  },
  () => {
    assert.strictEqual(test_reference.finalizeCount, 1);
  },

  'Weak reference',
  () => {
    const value = test_reference.createExternalWithFinalize();
    assert.strictEqual(test_reference.finalizeCount, 0);
    test_reference.createReference(value, 0);
    assert.strictEqual(test_reference.referenceValue, value);
  },
  () => {
    // Value should be GC'd because there is only a weak ref
    assert.strictEqual(test_reference.referenceValue, undefined);
    assert.strictEqual(test_reference.finalizeCount, 1);
    test_reference.deleteReference();
  },

  'Strong reference',
  () => {
    const value = test_reference.createExternalWithFinalize();
    assert.strictEqual(test_reference.finalizeCount, 0);
    test_reference.createReference(value, 1);
    assert.strictEqual(test_reference.referenceValue, value);
  },
  () => {
    // Value should NOT be GC'd because there is a strong ref
    assert.strictEqual(test_reference.finalizeCount, 0);
    test_reference.deleteReference();
  },
  () => {
    // Value should be GC'd because the strong ref was deleted
    assert.strictEqual(test_reference.finalizeCount, 1);
  },

  'Strong reference, increment then decrement to weak reference',
  () => {
    const value = test_reference.createExternalWithFinalize();
    assert.strictEqual(test_reference.finalizeCount, 0);
    test_reference.createReference(value, 1);
  },
  () => {
    // Value should NOT be GC'd because there is a strong ref
    assert.strictEqual(test_reference.finalizeCount, 0);
    assert.strictEqual(test_reference.incrementRefcount(), 2);
  },
  () => {
    // Value should NOT be GC'd because there is a strong ref
    assert.strictEqual(test_reference.finalizeCount, 0);
    assert.strictEqual(test_reference.decrementRefcount(), 1);
  },
  () => {
    // Value should NOT be GC'd because there is a strong ref
    assert.strictEqual(test_reference.finalizeCount, 0);
    assert.strictEqual(test_reference.decrementRefcount(), 0);
  },
  () => {
    // Value should be GC'd because the ref is now weak!
    assert.strictEqual(test_reference.finalizeCount, 1);
    test_reference.deleteReference();
  },
  () => {
    // Value was already GC'd
    assert.strictEqual(test_reference.finalizeCount, 1);
  },
]);

// This test creates a napi_ref on an object that has
// been wrapped by napi_wrap and for which the finalizer
// for the wrap calls napi_delete_ref on that napi_ref.
//
// Since both the wrap and the reference use the same
// object the finalizer for the wrap and reference
// may run in the same gc and in any order.
//
// It does that to validate that napi_delete_ref can be
// called before the finalizer has been run for the
// reference (there is a finalizer behind the scenes even
// though it cannot be passed to napi_create_reference).
//
// Since the order is not guarranteed, run the
// test a number of times maximize the chance that we
// get a run with the desired order for the test.
//
// 1000 reliably recreated the problem without the fix
// required to ensure delete could be called before
// the finalizer in manual testing.
for (let i = 0; i < 1000; i++) {
  const wrapObject = new Object();
  test_reference.validateDeleteBeforeFinalize(wrapObject);
  global.gc();
}
