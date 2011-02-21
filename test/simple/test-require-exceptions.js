var common = require('../common');
var assert = require('assert');

// A module with an error in it should throw
assert.throws(function() {
  require(common.fixturesDir + '/throws_error');
});

// Requiring the same module again should throw as well
assert.throws(function() {
  require(common.fixturesDir + '/throws_error');
});
