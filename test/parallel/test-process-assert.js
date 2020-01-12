'use strict';
const common = require('../common');
const assert = require('assert');

common.expectWarning(
  'DeprecationWarning',
  'process.assert() is deprecated. Please use the `assert` module instead.',
  'DEP0100'
);

assert.strictEqual(process.assert(1, 'error'), undefined);
assert.throws(() => {
  process.assert(undefined, 'errorMessage');
}, {
  code: 'ERR_ASSERTION',
  name: 'Error',
  message: 'errorMessage'
});
assert.throws(() => {
  process.assert(false);
}, {
  code: 'ERR_ASSERTION',
  name: 'Error',
  message: 'assertion error'
});
