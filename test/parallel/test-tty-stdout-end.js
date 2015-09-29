'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
var common = require('../common');
var assert = require('assert');

var shouldThrow = function() {
  process.stdout.end();
};

var validateError = function(e) {
  return e instanceof Error &&
    e.message === 'process.stdout cannot be closed.';
};

assert.throws(shouldThrow, validateError);
