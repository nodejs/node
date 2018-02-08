'use strict';
const common = require('../common');
const fs = require('fs');

// This test ensures that input for fchmod is valid, testing for valid
// inputs for fd and mode

// Check input type
['', false, null, undefined, {}, [], Infinity, -1].forEach((i) => {
  common.expectsError(
    () => fs.fchmod(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.fchmodSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );

  common.expectsError(
    () => fs.fchmod(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "mode" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.fchmodSync(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "mode" argument must be of type integer'
    }
  );
});

// Check for mode values range
const modeUpperBoundaryValue = 0o777;
fs.fchmod(1, modeUpperBoundaryValue, common.mustCall());
fs.fchmodSync(1, modeUpperBoundaryValue);

// umask of 0o777 is equal to 775
const modeOutsideUpperBoundValue = 776;
common.expectsError(
  () => fs.fchmod(1, modeOutsideUpperBoundValue),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "mode" is out of range.'
  }
);
common.expectsError(
  () => fs.fchmodSync(1, modeOutsideUpperBoundValue),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "mode" is out of range.'
  }
);
