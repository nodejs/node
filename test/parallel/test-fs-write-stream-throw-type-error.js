'use strict';
const common = require('../common');
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
  common.expectsError(
    () => {
      fs.createWriteStream(path, opt);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
};

createWriteStreamErr(example, 123);
createWriteStreamErr(example, 0);
createWriteStreamErr(example, true);
createWriteStreamErr(example, false);
