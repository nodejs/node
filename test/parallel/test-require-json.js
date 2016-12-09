'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');

try {
  require(path.join(common.fixturesDir, 'invalid.json'));
} catch (err) {
  const re = /test[/\\]fixtures[/\\]invalid.json: Unexpected string/;
  const i = err.message.match(re);
  assert.notStrictEqual(null, i, 'require() json error should include path');
}
