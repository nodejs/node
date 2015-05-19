'use strict';
var assert = require('assert');

try {
  require('../fixtures/invalid.json');
} catch (err) {
  var re = /test[\/\\]fixtures[\/\\]invalid.json: Unexpected string/;
  var i = err.message.match(re);
  assert(null !== i, 'require() json error should include path');
}
