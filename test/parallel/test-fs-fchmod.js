'use strict';
const assert = require('assert');
const fs = require('fs');

// This test ensures that input for fchmod is valid, testing for valid
// inputs for fd and mode

// Check input type
[false, null, undefined, {}, [], ''].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
});


[false, null, {}, []].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
  };
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

assert.throws(() => fs.fchmod(1, '123x'), {
  code: 'ERR_INVALID_ARG_VALUE'
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  };

  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[NaN, Infinity].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});

[1.5].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  };
  assert.throws(() => fs.fchmod(input), errObj);
  assert.throws(() => fs.fchmodSync(input), errObj);
  assert.throws(() => fs.fchmod(1, input), errObj);
  assert.throws(() => fs.fchmodSync(1, input), errObj);
});
