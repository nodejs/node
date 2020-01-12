'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');
const fs = require('fs');

// This test ensures that input for fchmod is valid, testing for valid
// inputs for fd and mode

// Check input type
[false, null, undefined, {}, [], ''].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "fd" argument must be of type number.' +
             common.invalidArgTypeHelper(input)
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
});


[false, null, undefined, {}, [], '', '123x'].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
    message: 'The argument \'mode\' must be a 32-bit unsigned integer or an ' +
             `octal string. Received ${util.inspect(input)}`
  };
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "fd" is out of range. It must be >= 0 && <= ' +
             `2147483647. Received ${input}`
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "mode" is out of range. It must be >= 0 && <= ' +
             `4294967295. Received ${input}`
  };

  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[NaN, Infinity].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
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
    name: 'RangeError',
    message: 'The value of "fd" is out of range. It must be an integer. ' +
             `Received ${input}`
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  errObj.message = errObj.message.replace('fd', 'mode');
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});
