'use strict';
// Addons: binding, binding_vtable

const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  const binding = require(addonPath);
  binding();
  setTimeout(() => {}, 100);
} else {
  const { status, stderr } = spawnTestSync();
  const stderrText = stderr ? stderr.toString().trim() : '';
  assert.strictEqual(status, 0, stderrText);
}
