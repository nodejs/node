'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

common.requireNoPackageJSONAbove();

const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures.js');

const policyFilepath = fixtures.path('policy-manifest', 'invalid.json');

const result = spawnSync(process.execPath, [
  '--experimental-policy',
  policyFilepath,
  './fhqwhgads.js',
]);

assert.notStrictEqual(result.status, 0);
const stderr = result.stderr.toString();
assert.match(stderr, /ERR_MANIFEST_INVALID_SPECIFIER/);
assert.match(stderr, /pattern needs to have a single trailing "\*"/);
