'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

assert.throws(function() {
  require('internal/freelist');
}, /^Error: Cannot find module 'internal\/freelist'/);

assert.strictEqual(
  require(fixtures.path('internal-modules')),
  42
);
