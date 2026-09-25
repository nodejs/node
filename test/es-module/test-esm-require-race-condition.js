'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');

assert.throws(
  () => require(fixtures.path('import-require-cycle/race-condition.cjs')),
  { code: 'ERR_REQUIRE_ESM_RACE_CONDITION' },
);
