'use strict';
require('../common');

// This test ensures that createWriteStream throws a RangeError when
// an out of range mode is passed in the options, as reported here:
// https://github.com/nodejs/node/issues/37430

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const example = path.join(tmpdir.path, 'dummy');

assert.throws(
  () => {
    fs.createWriteStream(example, { mode: 2176057344 });
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError'
  });
