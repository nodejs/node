'use strict';
require('../common');
const assert = require('assert');

const shouldThrow = () => {
  process.stdout.end();
};

const validateError = (e) => {
  return e instanceof Error &&
    e.message === 'process.stdout cannot be closed.';
};

assert.throws(shouldThrow, validateError);
