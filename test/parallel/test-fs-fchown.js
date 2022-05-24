'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

function testFd(input, errObj) {
  assert.throws(() => fs.fchown(input), errObj);
  assert.throws(() => fs.fchownSync(input), errObj);
}

function testUid(input, errObj) {
  assert.throws(() => fs.fchown(1, input), errObj);
  assert.throws(() => fs.fchownSync(1, input), errObj);
}

function testGid(input, errObj) {
  assert.throws(() => fs.fchown(1, 1, input), errObj);
  assert.throws(() => fs.fchownSync(1, 1, input), errObj);
}

['', false, null, undefined, {}, []].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  };
  testFd(input, errObj);
  testUid(input, errObj);
  testGid(input, errObj);
});

[Infinity, NaN].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  };
  testFd(input, errObj);
  testUid(input, errObj);
  testGid(input, errObj);
});

[-2, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  };
  testFd(input, errObj);
  testUid(input, errObj);
  testGid(input, errObj);
});
