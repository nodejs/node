'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

const main = fixtures.path('policy-preload', 'main.js');
const allowed = fixtures.path('policy-preload', 'allowed.js');
const always = fixtures.path('policy-preload', 'always.js');
const banned = fixtures.path('policy-preload', 'banned.js');
const explodes = fixtures.path('policy-preload', 'explodes.js');
const policy = fixtures.path('policy-preload', 'policy.json');
const policyAllowedTrue = fixtures.path(
  'policy-preload',
  'policy-allowed-true.json');
const policyAllowedNull = fixtures.path(
  'policy-preload',
  'policy-allowed-null.json');
const policyAllowedEmpty = fixtures.path(
  'policy-preload',
  'policy-allowed-empty.json');
const policyAlwaysEmpty = fixtures.path(
  'policy-preload',
  'policy-always-empty.json');
const policyAlwaysNull = fixtures.path(
  'policy-preload',
  'policy-always-null.json');
{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policy,
      main,
    ]
  );
  assert.strictEqual(status, 0);
  assert.strictEqual(`${stdout}`, 'from always.js\nfrom main.js\n');
}
{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policy,
      '--require', allowed,
      main,
    ]
  );
  assert.strictEqual(status, 0);
  assert.strictEqual(
    `${stdout}`,
    'from always.js\nfrom allowed.js\nfrom main.js\n');
}
{
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policy,
      '--require', allowed,
      '--require', explodes,
      main,
    ]
  );
  assert.strictEqual(status, 1);
}
{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policy,
      '--require', banned,
      main,
    ]
  );
  assert.strictEqual(status, 1);
  assert.strictEqual(`${stdout}`, 'from always.js\n');
}
{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policy,
      '--require', banned,
      '--require', allowed,
      main,
    ]
  );
  assert.strictEqual(status, 1);
  assert.strictEqual(`${stdout}`, 'from always.js\n');
}
{
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policy,
      '--require', explodes,
      main,
    ]
  );
  assert.strictEqual(status, 1);
  assert.strictEqual(`${stdout}`, 'from always.js\n');
}
{
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policyAllowedTrue,
      '--require', explodes,
      main,
    ]
  );
  assert.strictEqual(status, 0);
}
{
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policyAllowedEmpty,
      '--require', always,
      main,
    ]
  );
  assert.strictEqual(status, 1);
}

{
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policyAllowedNull,
      '--require', always,
      main,
    ]
  );
  assert.strictEqual(status, 1);
}
{
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policyAlwaysNull,
      '--require', always,
      main,
    ]
  );
  assert.strictEqual(status, 1);
}
{
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', policyAlwaysEmpty,
      '--require', always,
      main,
    ]
  );
  assert.strictEqual(status, 1);
}
