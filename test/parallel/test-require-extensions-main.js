'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const fixturesRequire =
  require(fixtures.path('require-bin', 'bin', 'req.js'));

assert.strictEqual(
  fixturesRequire,
  '',
  'test-require-extensions-main failed to import fixture requirements'
);
