'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const length = 4;

[true, null, undefined, () => {}, {}].forEach((value) => {
  common.expectsError(() => {
    fs.readSync(value,
                Buffer.allocUnsafe(length),
                0,
                length,
                0);
  }, { code: 'ERR_INVALID_ARG_TYPE', type: TypeError,
       message: 'The "fd" argument must be of type integer' });
});

common.expectsError(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(length),
              -1,
              length,
              0);
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError,
     message: 'The value of "offset" is out of range.' });

common.expectsError(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(length),
              0,
              -1,
              0);
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError,
     message: 'The value of "length" is out of range.' });
