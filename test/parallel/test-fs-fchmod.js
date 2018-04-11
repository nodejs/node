'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// This test ensures that input for fchmod is valid, testing for valid
// inputs for fd and mode

// Check input type
[false, null, undefined, {}, [], ''].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError [ERR_INVALID_ARG_TYPE]',
    message: 'The "fd" argument must be of type number. Received type ' +
             typeof input
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  errObj.message = errObj.message.replace('fd', 'mode');
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "fd" is out of range. It must be >= 0 && < ' +
             `${2 ** 32}. Received ${input}`
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  errObj.message = errObj.message.replace('fd', 'mode');
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[NaN, Infinity].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "fd" is out of range. It must be an integer. ' +
             `Received ${input}`
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  errObj.message = errObj.message.replace('fd', 'mode');
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[1.5].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "fd" is out of range. It must be an integer. ' +
             `Received ${input}`
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  errObj.message = errObj.message.replace('fd', 'mode');
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

// Check for mode values range
const modeUpperBoundaryValue = 0o777;
fs.fchmod(1, modeUpperBoundaryValue, common.mustCall());
fs.fchmodSync(1, modeUpperBoundaryValue);

// umask of 0o777 is equal to 775
const modeOutsideUpperBoundValue = 776;
assert.throws(
  () => fs.fchmod(1, modeOutsideUpperBoundValue),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "mode" is out of range. Received 776'
  }
);
assert.throws(
  () => fs.fchmodSync(1, modeOutsideUpperBoundValue),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "mode" is out of range. Received 776'
  }
);
