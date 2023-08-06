'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.requireNoPackageJSONAbove();

const fixtures = require('../common/fixtures');

const assert = require('assert');
const { spawnSync } = require('child_process');

const mainPath = fixtures.path('policy', 'crypto-hash-tampering', 'main.js');
const policyPath = fixtures.path(
  'policy',
  'crypto-hash-tampering',
  'policy.json');
const { status, stderr } =
    spawnSync(process.execPath, ['--experimental-policy', policyPath, mainPath], { encoding: 'utf8' });
assert.strictEqual(status, 1);
assert(stderr.includes('sha384-Bnp/T8gFNzT9mHj2G/AeuMH8LcAQ4mljw15nxBNl5yaGM7VgbMzDT7O4+dXZTJJn'));
