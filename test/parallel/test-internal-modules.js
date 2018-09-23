'use strict';
var common = require('../common');
var assert = require('assert');

assert.throws(function() {
  require('internal/freelist');
});

assert(require('../fixtures/internal-modules') === 42);
