'use strict';
// Tests that require() of an ES module whose earlier import() failed during
// instantiation throws ERR_REQUIRE_ESM_RACE_CONDITION.
const common = require('../common');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');

const url = fixtures.fileURL('es-modules/require-after-failed-import/mismatched.mjs');

assert.rejects(import(url), { name: 'SyntaxError' }).then(common.mustCall(() => {
  assert.throws(
    () => require(fixtures.path('es-modules/require-after-failed-import/mismatched.mjs')),
    { code: 'ERR_REQUIRE_ESM_RACE_CONDITION' },
  );
}));
