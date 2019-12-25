'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');

const example = path.join(tmpdir.path, 'dummy');

tmpdir.refresh();
// Should not throw.
fs.createWriteStream(example, undefined);
fs.createWriteStream(example, null);
fs.createWriteStream(example, 'utf8');
fs.createWriteStream(example, { encoding: 'utf8' });

const createWriteStreamErr = (path, opt) => {
  assert.throws(
    () => {
      fs.createWriteStream(path, opt);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
};

createWriteStreamErr(example, 123);
createWriteStreamErr(example, 0);
createWriteStreamErr(example, true);
createWriteStreamErr(example, false);
