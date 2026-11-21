// This fixture is used to test that customization hooks are not enabled in the ShadowRealm.

'use strict';
const assert = require('assert');

async function workInChildProcess() {
  // Assert that the process is running with custom hooks.
  const moduleNamespace = await import('file:///42.mjs');
  assert.strictEqual(moduleNamespace.default, 42);

  const realm = new ShadowRealm();
  await assert.rejects(realm.importValue('file:///42.mjs', 'default'), TypeError);
}

workInChildProcess();
