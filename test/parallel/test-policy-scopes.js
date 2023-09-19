'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.requireNoPackageJSONAbove();

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

{
  const dep = fixtures.path('policy', 'main.mjs');
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-scopes-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 0);
}
{
  const dep = fixtures.path('policy', 'multi-deps.js');
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-scopes-and-resources-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 0);
}
