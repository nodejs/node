// Flags: --expose_internals
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const {
  requireDepth
} = require('internal/modules/cjs/helpers');

// Module one loads two too so the expected depth for two is, well, two.
assert.strictEqual(requireDepth, 0);
const one = require(fixtures.path('module-require-depth', 'one'));
const two = require(fixtures.path('module-require-depth', 'two'));
assert.deepStrictEqual(one, { requireDepth: 1 });
assert.deepStrictEqual(two, { requireDepth: 2 });
assert.strictEqual(requireDepth, 0);
