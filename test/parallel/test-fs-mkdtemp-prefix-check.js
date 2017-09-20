'use strict';
const common = require('../common');
const fs = require('fs');

const prefixValues = [undefined, null, 0, true, false, 1, ''];

function fail(value) {
  common.expectsError(
    () => {
      fs.mkdtempSync(value, {});
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
}

function failAsync(value) {
  common.expectsError(
    () => {
      fs.mkdtemp(value, common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
}

prefixValues.forEach((prefixValue) => {
  fail(prefixValue);
  failAsync(prefixValue);
});
