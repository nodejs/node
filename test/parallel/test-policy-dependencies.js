'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

const dep = fixtures.path('policy', 'parent.js');
{
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-redirect-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 0);
}
{
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-redirect-builtin-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 0);
}
{
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-redirect-unknown-builtin-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 1);
}
{
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-wildcard-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 0);
}
{
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-empty-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 1);
}
{
  const depPolicy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-missing-policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ]
  );
  assert.strictEqual(status, 1);
}
