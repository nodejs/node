'use strict';
var common = require('../common');
var assert = require('assert');

try {
  require('../fixtures/invalid.json');
} catch (err) {
  var re = common.engineSpecificMessage({
    v8: /test[\/\\]fixtures[\/\\]invalid.json: Unexpected string/,
    chakracore : /test[\/\\]fixtures[\/\\]invalid.json: Expected '}'/
  });
  var i = err.message.match(re);
  assert(null !== i, 'require() json error should include path');
}
