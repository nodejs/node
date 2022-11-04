'use strict';

const common = require('../common');

if (!common.hasIntl) {
  common.skip('missing Intl');
}

const fixtures = require('../common/fixtures');

// This test ensures the correctness of the automated timezone upgrade PRs.

const { strictEqual } = require('assert');
const { readFileSync } = require('fs');

const expectedVersion = readFileSync(fixtures.path('tz-version.txt'), 'utf8').trim();
strictEqual(process.versions.tz, expectedVersion);
