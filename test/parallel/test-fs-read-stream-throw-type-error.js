'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');

const example = fixtures.path('x.txt');
// Should not throw.
fs.createReadStream(example, undefined);
fs.createReadStream(example, null);
fs.createReadStream(example, 'utf8');
fs.createReadStream(example, { encoding: 'utf8' });

const createReadStreamErr = (path, opt) => {
  common.expectsError(
    () => {
      fs.createReadStream(path, opt);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
};

createReadStreamErr(example, 123);
createReadStreamErr(example, 0);
createReadStreamErr(example, true);
createReadStreamErr(example, false);
