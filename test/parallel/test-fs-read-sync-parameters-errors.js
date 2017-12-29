'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = Buffer.from('xyz\n');

[true, null, undefined, () => {}, {}].forEach((value) => {
  common.expectsError(() => {
    fs.read(value,
            Buffer.allocUnsafe(expected.length),
            0,
            expected.length,
            0);
  }, { code: 'ERR_INVALID_ARG_TYPE', type: TypeError });
});

common.expectsError(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              -1,
              expected.length,
              0);
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError });

common.expectsError(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              0,
              -1,
              0);
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError });
