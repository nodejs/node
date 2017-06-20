'use strict';

const common = require('../common');
const fs = require('fs');

const encoding = 'foo-8';
const filename = 'bar.txt';
common.expectsError(
  fs.readFile.bind(fs, filename, { encoding }, common.mustNotCall()),
  { code: 'ERR_INVALID_OPT_VALUE_ENCODING', type: TypeError }
);
