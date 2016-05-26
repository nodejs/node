'use strict';
var common = require('../common');
var path = require('path');
var assert = require('assert');

try {
  require(path.join(common.fixturesDir, 'invalid.json'));
} catch (err) {
  var re = /test[\/\\]fixtures[\/\\]invalid.json: Unexpected string/;
  var i = err.message.match(re);
  assert(null !== i, 'require() json error should include path');
}
