'use strict';
// Addons: test_async, test_async_vtable

const common = require('../../common');
const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');
const test_async = require(addonPath);

const testException = 'test_async_cb_exception';

// Exception thrown from async completion callback.
// (Tested in a spawned process because the exception is fatal.)
if (isInvokedAsChild) {
  test_async.Test(1, {}, common.mustCall(function() {
    throw new Error(testException);
  }));
} else {
  const p = spawnTestSync();
  assert.ifError(p.error);
  assert.ok(p.stderr.toString().includes(testException));

  // Successful async execution and completion callback.
  test_async.Test(5, {}, common.mustCall(function(err, val) {
    assert.strictEqual(err, null);
    assert.strictEqual(val, 10);
    process.nextTick(common.mustCall());
  }));

  // Async work item cancellation with callback.
  test_async.TestCancel(common.mustCall());
}
