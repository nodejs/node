'use strict';
// Addons: binding, binding_vtable

const { addonPath, isInvokedAsChild, spawnTestSync } = require('../../common/addon-test');
const assert = require('assert');

if (isInvokedAsChild) {
  require(addonPath);
} else {
  const { stdout, status, signal } = spawnTestSync();
  assert.strictEqual(status, 0, `process exited with status(${status}) and signal(${signal})`);
  assert.strictEqual(stdout.toString().trim(), 'cleanup(42)');
}
