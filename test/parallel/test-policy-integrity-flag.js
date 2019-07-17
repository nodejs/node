'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const crypto = require('crypto');

const depPolicy = fixtures.path('policy', 'dep-policy.json');
const dep = fixtures.path('policy', 'dep.js');

const emptyHash = crypto.createHash('sha512');
emptyHash.update('');
const emptySRI = `sha512-${emptyHash.digest('base64')}`;
const policyHash = crypto.createHash('sha512');
policyHash.update(fs.readFileSync(depPolicy));
const depPolicySRI = `sha512-${policyHash.digest('base64')}`;
{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--policy-integrity', emptySRI,
      '--experimental-policy', depPolicy, dep,
    ]
  );

  assert.ok(stderr.includes('ERR_MANIFEST_ASSERT_INTEGRITY'));
  assert.strictEqual(status, 1);
}
{
  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--policy-integrity', '',
      '--experimental-policy', depPolicy, dep,
    ]
  );

  assert.ok(stderr.includes('--policy-integrity'));
  assert.strictEqual(status, 9);
}
{
  const { status } = spawnSync(
    process.execPath,
    [
      '--policy-integrity', depPolicySRI,
      '--experimental-policy', depPolicy, dep,
    ]
  );

  assert.strictEqual(status, 0);
}
