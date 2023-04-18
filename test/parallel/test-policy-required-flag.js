'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.requireNoPackageJSONAbove();

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

const depPolicy = fixtures.path('policy', 'dep-policy.json');
const dep = fixtures.path('policy', 'dep.js');

{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--policy-required',
      '--experimental-policy', depPolicy, dep,
    ]
  );

  assert.strictEqual(status, 0, `status: ${status}\nstderr: ${stderr}`);
}
{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--policy-required',
      dep,
    ]
  );

  assert.strictEqual(status, 9, `status: ${status}\nstderr: ${stderr}`);
}
