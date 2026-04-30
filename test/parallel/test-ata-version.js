'use strict';

require('../common');
const assert = require('assert');

// Verify that ata-validator is available in process.versions
assert.ok(process.versions.ata, 'process.versions.ata should be defined');
assert.match(process.versions.ata, /^\d+\.\d+\.\d+$/,
             'process.versions.ata should be a semver string');
