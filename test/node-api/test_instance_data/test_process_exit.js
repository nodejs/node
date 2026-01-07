'use strict';
// Addons: test_ref_then_set, test_ref_then_set_vtable
// Addons: test_set_then_ref, test_set_then_ref_vtable

const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  require(addonPath);
} else {
  // Make sure that process exit is clean when the instance data has
  // references to JS objects.
  const child = spawnTestSync();
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.stderr.toString(), 'addon_free');
}
