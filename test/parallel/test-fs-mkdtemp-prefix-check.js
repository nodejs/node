'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const expectedError = /^TypeError: filename prefix is required$/;
const prefixValues = [undefined, null, 0, true, false, 1, ''];

function fail(value) {
  assert.throws(
    () => fs.mkdtempSync(value, {}),
    expectedError
  );
}

function failAsync(value) {
  assert.throws(
    () => fs.mkdtemp(value, common.mustNotCall()), expectedError
  );
}

prefixValues.forEach((prefixValue) => {
  fail(prefixValue);
  failAsync(prefixValue);
});
