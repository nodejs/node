// Flags: --expose-internals
'use strict';
const common = require('../common');
const {
  validateInteger
} = require('internal/validators');
const { MAX_SAFE_INTEGER, MIN_SAFE_INTEGER } = Number;
const outOfRangeError = {
  code: 'ERR_OUT_OF_RANGE',
  type: RangeError
};

// validateInteger() defaults to validating safe integers.
validateInteger(MAX_SAFE_INTEGER, 'foo');
validateInteger(MIN_SAFE_INTEGER, 'foo');
common.expectsError(() => {
  validateInteger(MAX_SAFE_INTEGER + 1, 'foo');
}, outOfRangeError);
common.expectsError(() => {
  validateInteger(MIN_SAFE_INTEGER - 1, 'foo');
}, outOfRangeError);

// validateInteger() works with unsafe integers.
validateInteger(MAX_SAFE_INTEGER + 1, 'foo', 0, MAX_SAFE_INTEGER + 1);
validateInteger(MIN_SAFE_INTEGER - 1, 'foo', MIN_SAFE_INTEGER - 1);
