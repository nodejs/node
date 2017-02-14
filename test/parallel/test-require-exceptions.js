'use strict';
const common = require('../common');
const assert = require('assert');

// A module with an error in it should throw
assert.throws(function() {
  require(common.fixturesDir + '/throws_error');
}, /^Error: blah$/);

// Requiring the same module again should throw as well
assert.throws(function() {
  require(common.fixturesDir + '/throws_error');
}, /^Error: blah$/);

// Requiring a module that does not exist should throw an
// error with its `code` set to MODULE_NOT_FOUND
assertModuleNotFound('/DOES_NOT_EXIST');

assertExists('/module-require/not-found/trailingSlash.js');
assertExists('/module-require/not-found/node_modules/module1/package.json');
assertModuleNotFound('/module-require/not-found/trailingSlash');

function assertModuleNotFound(path) {
  assert.throws(function() {
    require(common.fixturesDir + path);
  }, function(e) {
    assert.strictEqual(e.code, 'MODULE_NOT_FOUND');
    return true;
  });
}

function assertExists(fixture) {
  assert(common.fileExists(common.fixturesDir + fixture));
}
