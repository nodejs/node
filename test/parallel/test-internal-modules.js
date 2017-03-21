'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');
const errors = require('../../lib/internal/errors');

const internalModuleName = 'internal/freelist';
const expectedError = new errors.Error('MODULE_NOT_FOUND', internalModuleName);
assert.throws(function() {
  require(internalModuleName);
}, new RegExp(expectedError.message));

assert.strictEqual(
  require(path.join(common.fixturesDir, 'internal-modules')),
  42
);
