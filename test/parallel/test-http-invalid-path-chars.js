'use strict';

require('../common');
const assert = require('assert');
const http = require('http');

const theExperimentallyDeterminedNumber = 39;

for (let i = 0; i <= theExperimentallyDeterminedNumber; i++) {
  const prefix = 'a'.repeat(i);
  for (let i = 0; i <= 32; i++) {
    assert.throws(() => {
      http.request({ path: prefix + String.fromCodePoint(i) }, assert.fail);
    }, {
      code: 'ERR_UNESCAPED_CHARACTERS',
      name: 'TypeError',
      message: 'Request path contains unescaped characters'
    });
  }
}
