'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

const commonArgs = [
  '--experimental-shadow-realm',
  '--no-warnings',
];

async function main() {
  // Verifies that customization hooks are not enabled in the ShadowRealm.
  const child = await common.spawnPromisified(process.execPath, [
    ...commonArgs,
    '--require',
    fixtures.path('es-module-shadow-realm', 'register-hooks.js'),
    fixtures.path('es-module-shadow-realm', 'custom-hooks.js'),
  ]);

  assert.strictEqual(child.stderr, '');
  assert.strictEqual(child.code, 0);
}

main().then(common.mustCall());
