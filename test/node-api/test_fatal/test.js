'use strict';
// Addons: test_fatal, test_fatal_vtable

const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');
const test_fatal = require(addonPath);

// Test in a child process because the test code will trigger a fatal error
// that crashes the process.
if (isInvokedAsChild) {
  test_fatal.Test();
} else {
  const p = spawnTestSync();
  assert.ifError(p.error);
  assert.ok(p.stderr.toString().includes(
    'FATAL ERROR: test_fatal::Test fatal message'));
  assert.ok(p.status === 134 || p.signal === 'SIGABRT');
}
