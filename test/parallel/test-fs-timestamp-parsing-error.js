'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');

[Infinity, -Infinity, NaN].forEach((input) => {
  common.expectsError(
    () => {
      fs._toUnixTimestamp(input);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
});

common.expectsError(
  () => {
    fs._toUnixTimestamp({});
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });

const okInputs = [1, -1, '1', '-1', Date.now()];
okInputs.forEach((input) => {
  assert.doesNotThrow(() => fs._toUnixTimestamp(input));
});
