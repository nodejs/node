'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const expectedError = common.expectsError({
  code: 'ERR_UNESCAPED_CHARACTERS',
  type: TypeError,
  message: 'Request path contains unescaped characters'
}, 1320);
const theExperimentallyDeterminedNumber = 39;

function fail(path) {
  assert.throws(() => {
    http.request({ path }, assert.fail);
  }, expectedError);
}

for (let i = 0; i <= theExperimentallyDeterminedNumber; i++) {
  const prefix = 'a'.repeat(i);
  for (let i = 0; i <= 32; i++) {
    fail(prefix + String.fromCodePoint(i));
  }
}
