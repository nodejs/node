'use strict';
var common = require('../common');
/*
var path = require('path');
var assert = require('assert');

assert.throws(function() {
  require('internal/freelist');
});

assert.strictEqual(
  require(path.join(common.fixturesDir, 'internal-modules')),
  42
);
*/

common.expectWarning('DeprecationWarning', [
  'Accessing internal modules of Node using `require()` is strongly ' +
  'discouraged and will be disabled in the future. ' +
  'Please make sure that your dependencies are up to date.'
]);

require('internal/freelist');
