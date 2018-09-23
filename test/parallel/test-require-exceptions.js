'use strict';
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

// Requiring a module that does not exist should throw an
// error with its `code` set to MODULE_NOT_FOUND
assert.throws(function() {
  require(common.fixturesDir + '/DOES_NOT_EXIST');
}, function(e) {
  assert.equal('MODULE_NOT_FOUND', e.code);
  return true;
});
