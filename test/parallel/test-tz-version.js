'use strict';

require('../common');
const { path } = require('../common/fixtures');

// This test ensures the correctness of the automated timezone upgrade PRs.

const { strictEqual } = require('assert');
const { readFileSync } = require('fs');

const expectedVersion = readFileSync(path('tz-version.txt'), 'utf8').trim();
strictEqual(process.versions.tz, expectedVersion);
