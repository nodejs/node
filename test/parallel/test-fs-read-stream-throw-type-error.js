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

const createReadStreamErr = (path, opt) => {
  common.expectsError(
    () => {
      fs.createReadStream(path, opt);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
};

createReadStreamErr(example, 123);
createReadStreamErr(example, 0);
createReadStreamErr(example, true);
createReadStreamErr(example, false);

// Should also throw on NaN (for https://github.com/nodejs/node/pull/19732)
createReadStreamErr(example, { start: NaN });
createReadStreamErr(example, { end: NaN });
createReadStreamErr(example, { start: NaN, end: NaN });
