'use strict';

const common = require('../common');
common.requireNoPackageJSONAbove();

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

const dep = fixtures.path('policy', 'process-binding', 'app.js');
const depPolicy = fixtures.path(
  'policy',
  'process-binding',
  'policy.json');
const { status } = spawnSync(
  process.execPath,
  [
    '--experimental-policy', depPolicy, dep,
  ],
  {
    stdio: 'inherit'
  },
);
assert.strictEqual(status, 0);
