'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const encoding = 'foo-8';
const filename = 'bar.txt';

assert.throws(
  fs.readFile.bind(fs, filename, { encoding }, common.noop),
  new RegExp(`Error: Unknown encoding: ${encoding}$`)
);
