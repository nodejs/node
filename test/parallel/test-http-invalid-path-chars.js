'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const expectedError = /^TypeError: Request path contains unescaped characters$/;
const theExperimentallyDeterminedNumber = 39;

function fail(path) {
  assert.throws(() => {
    http.request({ path }, common.fail);
  }, expectedError);
}

for (let i = 0; i <= theExperimentallyDeterminedNumber; i++) {
  const prefix = 'a'.repeat(i);
  for (let i = 0; i <= 32; i++) {
    fail(prefix + String.fromCodePoint(i));
  }
}
