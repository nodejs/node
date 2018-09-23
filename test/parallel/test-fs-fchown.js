'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

function test(input, errObj) {
  assert.throws(() => fs.fchown(input), errObj);
  assert.throws(() => fs.fchownSync(input), errObj);
  errObj.message = errObj.message.replace('fd', 'uid');
  assert.throws(() => fs.fchown(1, input), errObj);
  assert.throws(() => fs.fchownSync(1, input), errObj);
  errObj.message = errObj.message.replace('uid', 'gid');
  assert.throws(() => fs.fchown(1, 1, input), errObj);
  assert.throws(() => fs.fchownSync(1, 1, input), errObj);
}

['', false, null, undefined, {}, []].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError [ERR_INVALID_ARG_TYPE]',
    message: 'The "fd" argument must be of type number. Received type ' +
             typeof input
  };
  test(input, errObj);
});

[Infinity, NaN].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "fd" is out of range. It must be an integer. ' +
             `Received ${input}`
  };
  test(input, errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "fd" is out of range. It must be ' +
             `>= 0 && < 4294967296. Received ${input}`
  };
  test(input, errObj);
});
