'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');

try {
  require(path.join(common.fixturesDir, 'invalid.json'));
} catch (err) {
  assert.ok(
    /test[/\\]fixtures[/\\]invalid.json: Unexpected string/.test(err.message),
    'require() json error should include path');
}
