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

[true, null, undefined, () => {}, {}].forEach((value) => {
  common.expectsError(() => {
    fs.read(value,
            Buffer.allocUnsafe(expected.length),
            0,
            expected.length,
            0,
            common.mustNotCall());
  }, { code: 'ERR_INVALID_ARG_TYPE', type: TypeError,
       message: 'The "fd" argument must be of type integer' });
});

common.expectsError(() => {
  fs.read(fd,
          Buffer.allocUnsafe(expected.length),
          -1,
          expected.length,
          0,
          common.mustNotCall());
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError,
     message: 'The value of "offset" is out of range.' });

common.expectsError(() => {
  fs.read(fd,
          Buffer.allocUnsafe(expected.length),
          0,
          -1,
          0,
          common.mustNotCall());
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError,
     message: 'The value of "length" is out of range.' });


common.expectsError(
  () => fs.readSync(fd, expected.length, 0, 'utf-8'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "buffer" argument must be one of type Buffer or Uint8Array'
  }
);

[true, null, undefined, () => {}, {}].forEach((value) => {
  common.expectsError(() => {
    fs.readSync(value,
                Buffer.allocUnsafe(expected.length),
                0,
                expected.length,
                0);
  }, { code: 'ERR_INVALID_ARG_TYPE', type: TypeError,
       message: 'The "fd" argument must be of type integer' });
});

common.expectsError(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              -1,
              expected.length,
              0);
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError,
     message: 'The value of "offset" is out of range.' });

common.expectsError(() => {
  fs.readSync(fd,
              Buffer.allocUnsafe(expected.length),
              0,
              -1,
              0);
}, { code: 'ERR_OUT_OF_RANGE', type: RangeError,
     message: 'The value of "length" is out of range.' });
