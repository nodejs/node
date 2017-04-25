'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');

const test_reference = require(`./build/${common.buildType}/test_reference`);

// This test script uses external values with finalizer callbacks
// in order to track when values get garbage-collected. Each invocation
// of a finalizer callback increments the finalizeCount property.
assert.strictEqual(test_reference.finalizeCount, 0);

{
  // External value without a finalizer
  let value = test_reference.createExternal();
  assert.strictEqual(test_reference.finalizeCount, 0);
  assert.strictEqual(typeof value, 'object');
  test_reference.checkExternal(value);
  value = null;
  global.gc();
  assert.strictEqual(test_reference.finalizeCount, 0);
}

{
  // External value with a finalizer
  let value = test_reference.createExternalWithFinalize();
  assert.strictEqual(test_reference.finalizeCount, 0);
  assert.strictEqual(typeof value, 'object');
  test_reference.checkExternal(value);
  value = null;
  global.gc();
  assert.strictEqual(test_reference.finalizeCount, 1);
}

{
  // Weak reference
  let value = test_reference.createExternalWithFinalize();
  assert.strictEqual(test_reference.finalizeCount, 0);
  test_reference.createReference(value, 0);
  assert.strictEqual(test_reference.referenceValue, value);
  value = null;
  global.gc(); // Value should be GC'd because there is only a weak ref
  assert.strictEqual(test_reference.referenceValue, undefined);
  assert.strictEqual(test_reference.finalizeCount, 1);
  test_reference.deleteReference();
}

{
  // Strong reference
  let value = test_reference.createExternalWithFinalize();
  assert.strictEqual(test_reference.finalizeCount, 0);
  test_reference.createReference(value, 1);
  assert.strictEqual(test_reference.referenceValue, value);
  value = null;
  global.gc(); // Value should NOT be GC'd because there is a strong ref
  assert.strictEqual(test_reference.finalizeCount, 0);
  test_reference.deleteReference();
  global.gc(); // Value should be GC'd because the strong ref was deleted
  assert.strictEqual(test_reference.finalizeCount, 1);
}

{
  // Strong reference, increment then decrement to weak reference
  let value = test_reference.createExternalWithFinalize();
  assert.strictEqual(test_reference.finalizeCount, 0);
  test_reference.createReference(value, 1);
  value = null;
  global.gc(); // Value should NOT be GC'd because there is a strong ref
  assert.strictEqual(test_reference.finalizeCount, 0);

  assert.strictEqual(test_reference.incrementRefcount(), 2);
  global.gc(); // Value should NOT be GC'd because there is a strong ref
  assert.strictEqual(test_reference.finalizeCount, 0);

  assert.strictEqual(test_reference.decrementRefcount(), 1);
  global.gc(); // Value should NOT be GC'd because there is a strong ref
  assert.strictEqual(test_reference.finalizeCount, 0);

  assert.strictEqual(test_reference.decrementRefcount(), 0);
  global.gc(); // Value should be GC'd because the ref is now weak!
  assert.strictEqual(test_reference.finalizeCount, 1);

  test_reference.deleteReference();
  global.gc(); // Value was already GC'd
  assert.strictEqual(test_reference.finalizeCount, 1);
}
