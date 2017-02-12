'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const encoding = 'foo-8';
const filename = 'bar.txt';
const expectedError = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE_ENCODING',
  type: TypeError,
});

assert.throws(
  fs.readFile.bind(fs, filename, { encoding }, common.mustNotCall()),
  expectedError
);
