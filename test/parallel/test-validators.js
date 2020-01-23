// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const {
  validateInteger
} = require('internal/validators');
const { MAX_SAFE_INTEGER, MIN_SAFE_INTEGER } = Number;
const outOfRangeError = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError'
};

// validateInteger() defaults to validating safe integers.
validateInteger(MAX_SAFE_INTEGER, 'foo');
validateInteger(MIN_SAFE_INTEGER, 'foo');
assert.throws(() => {
  validateInteger(MAX_SAFE_INTEGER + 1, 'foo');
}, outOfRangeError);
assert.throws(() => {
  validateInteger(MIN_SAFE_INTEGER - 1, 'foo');
}, outOfRangeError);

// validateInteger() works with unsafe integers.
validateInteger(MAX_SAFE_INTEGER + 1, 'foo', 0, MAX_SAFE_INTEGER + 1);
validateInteger(MIN_SAFE_INTEGER - 1, 'foo', MIN_SAFE_INTEGER - 1);
