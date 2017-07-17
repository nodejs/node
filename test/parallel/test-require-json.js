'use strict';
require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

try {
  require(fixtures.path('invalid.json'));
} catch (err) {
  assert.ok(
    /test[/\\]fixtures[/\\]invalid.json: Unexpected string/.test(err.message),
    'require() json error should include path');
}
