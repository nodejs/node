'use strict';
// Addons: test_fatal, test_fatal_vtable

const common = require('../../common');
const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');
const test_fatal = require(addonPath);

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (isInvokedAsChild) {
  test_fatal.TestThread();
  while (true) {
    // Busy loop to allow the work thread to abort.
  }
} else {
  const p = spawnTestSync();
  assert.ifError(p.error);
  assert.ok(p.stderr.toString().includes(
    'FATAL ERROR: work_thread foobar'));
  assert(common.nodeProcessAborted(p.status, p.signal));
}
