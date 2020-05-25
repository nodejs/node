'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const prefixValues = [undefined, null, 0, true, false, 1, ''];

function fail(value) {
  assert.throws(
    () => {
      fs.mkstempSync(value, {});
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
}

function failAsync(value) {
  assert.throws(
    () => {
      fs.mkstemp(value, common.mustNotCall());
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
}

prefixValues.forEach((prefixValue) => {
  fail(prefixValue);
  failAsync(prefixValue);
});
