'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.requireNoPackageJSONAbove();

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

const encodings = ['buffer', 'utf8', 'utf16le', 'latin1', 'base64', 'hex'];

for (const encoding of encodings) {
  const dep = fixtures.path('policy', 'crypto-default-encoding', 'parent.js');
  const depPolicy = fixtures.path(
    'policy',
    'crypto-default-encoding',
    'policy.json');
  const { status } = spawnSync(
    process.execPath,
    [
      '--experimental-policy', depPolicy, dep,
    ],
    {
      env: {
        ...process.env,
        DEFAULT_ENCODING: encoding
      }
    }
  );
  assert.strictEqual(status, 0);
}
