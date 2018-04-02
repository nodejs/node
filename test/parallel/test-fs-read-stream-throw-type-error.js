'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// This test ensures that fs.createReadStream throws a TypeError when invalid
// arguments are passed to it.

const fs = require('fs');

const example = fixtures.path('x.txt');
// Should not throw.
fs.createReadStream(example, undefined);
fs.createReadStream(example, null);
fs.createReadStream(example, 'utf8');
fs.createReadStream(example, { encoding: 'utf8' });

const createReadStreamErr = (path, opt, errorCode) => {
  common.expectsError(
    () => {
      fs.createReadStream(path, opt);
    },
    {
      code: errorCode,
      type: TypeError
    });
};

createReadStreamErr(example, 123, 'ERR_INVALID_ARG_TYPE');
createReadStreamErr(example, 0, 'ERR_INVALID_ARG_TYPE');
createReadStreamErr(example, true, 'ERR_INVALID_ARG_TYPE');
createReadStreamErr(example, false, 'ERR_INVALID_ARG_TYPE');

// Should also throw on NaN (for https://github.com/nodejs/node/pull/19732)
createReadStreamErr(example, { start: NaN }, 'ERR_OUT_OF_RANGE');
createReadStreamErr(example, { end: NaN }, 'ERR_OUT_OF_RANGE');
createReadStreamErr(example, { start: NaN, end: NaN }, 'ERR_OUT_OF_RANGE');
