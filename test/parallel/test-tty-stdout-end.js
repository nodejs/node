'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
const common = require('../common');
const assert = require('assert');

const shouldThrow = function() {
  process.stdout.end();
};

const validateError = function(e) {
  return e instanceof Error &&
    e.message === 'process.stdout cannot be closed.';
};

assert.throws(shouldThrow, validateError);
