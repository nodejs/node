'use strict';
var common = require('../common');
var path = require('path');
var assert = require('assert');

assert.throws(function() {
  require('internal/freelist');
});

assert(require(path.join(common.fixturesDir, 'internal-modules')) === 42);
