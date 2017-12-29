'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const length = 4;

[true, null, undefined, () => {}, {}].forEach((value) => {
  common.expectsError(() => {
    fs.read(value,
            Buffer.allocUnsafe(length),
            0,
            length,
            0,
            common.mustNotCall());
  }, { code: 'ERR_INVALID_ARG_TYPE', type: TypeError });
});

common.expectsError(() => {
  fs.read(fd,
          Buffer.allocUnsafe(length),
          -1,
          length,
          0,
          common.mustNotCall());
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError });

common.expectsError(() => {
  fs.read(fd,
          Buffer.allocUnsafe(length),
          0,
          -1,
          0,
          common.mustNotCall());
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError });
