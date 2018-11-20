'use strict';
const common = require('../common');
const fs = require('fs');

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
  fs._toUnixTimestamp(input);
});
