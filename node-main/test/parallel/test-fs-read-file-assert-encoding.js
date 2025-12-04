'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const encoding = 'foo-8';
const filename = 'bar.txt';
assert.throws(
  () => fs.readFile(filename, { encoding }, common.mustNotCall()),
  { code: 'ERR_INVALID_ARG_VALUE', name: 'TypeError' }
);
