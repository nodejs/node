'use strict';
const common = require('../../common');
const assert = require('assert');
const child_process = require('child_process');
const dc = require('diagnostics_channel');
const test_async = require(`./build/${common.buildType}/test_async`);

const testException = 'test_async_cb_exception';

// Exception thrown from async completion callback.
// (Tested in a spawned process because the exception is fatal.)
if (process.argv[2] === 'child') {
  test_async.Test(1, {}, common.mustCall(function() {
    throw new Error(testException);
  }));
  return;
}
const p = child_process.spawnSync(
  process.execPath, [ __filename, 'child' ]);
assert.ifError(p.error);
assert.ok(p.stderr.toString().includes(testException));

// Successful async execution and completion callback.
test_async.Test(5, {}, common.mustCall(function(err, val) {
  assert.strictEqual(err, null);
  assert.strictEqual(val, 10);
  process.nextTick(common.mustCall());
}));

// Async work item cancellation with callback.
const events = [];
const onThreadPoolWork = (event) => events.push(event);
dc.subscribe('threadpool.work', onThreadPoolWork);
test_async.TestCancel(common.mustCall(() => {
  dc.unsubscribe('threadpool.work', onThreadPoolWork);
  const event = events.find(({ type, started, ended }) =>
    type === 'node_api' && started === null && ended === null);
  assert.ok(event);
}));
