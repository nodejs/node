'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.tmpDir, 'dummy');

common.refreshTmpDir();

assert.doesNotThrow(() => {
  fs.createWriteStream(example, undefined);
});

assert.doesNotThrow(() => {
  fs.createWriteStream(example, null);
});

assert.doesNotThrow(() => {
  fs.createWriteStream(example, 'utf8');
});

assert.doesNotThrow(() => {
  fs.createWriteStream(example, { encoding: 'utf8' });
});

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
