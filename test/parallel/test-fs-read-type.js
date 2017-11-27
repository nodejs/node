'use strict';
const common = require('../common');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';

// Error must be thrown with string
common.expectsError(
  () => fs.read(fd, expected.length, 0, 'utf-8', common.mustNotCall()),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "buffer" argument must be one of type Buffer or Uint8Array'
  }
);

common.expectsError(
  () => fs.readSync(fd, expected.length, 0, 'utf-8'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "buffer" argument must be one of type Buffer or Uint8Array'
  }
);
