'use strict';

const common = require('../common');
const fs = require('fs');

['', false, null, undefined, {}, []].forEach((i) => {
  common.expectsError(
    () => fs.fchown(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type number'
    }
  );
  common.expectsError(
    () => fs.fchownSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type number'
    }
  );

  common.expectsError(
    () => fs.fchown(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "uid" argument must be of type number'
    }
  );
  common.expectsError(
    () => fs.fchownSync(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "uid" argument must be of type number'
    }
  );

  common.expectsError(
    () => fs.fchown(1, 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "gid" argument must be of type number'
    }
  );
  common.expectsError(
    () => fs.fchownSync(1, 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "gid" argument must be of type number'
    }
  );
});

[-1, 0xFFFFFFFF + 1].forEach((i) => {
  common.expectsError(
    () => fs.fchown(i),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The "fd" argument is out of range'
    }
  );
  common.expectsError(
    () => fs.fchownSync(i),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The "fd" argument is out of range'
    }
  );
});

common.expectsError(
  () => fs.fchown(1, -1),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The "uid" argument is out of range'
  }
);
common.expectsError(
  () => fs.fchownSync(1, -1),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The "uid" argument is out of range'
  }
);

common.expectsError(
  () => fs.fchown(1, 1, -1),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The "gid" argument is out of range'
  }
);
common.expectsError(
  () => fs.fchownSync(1, 1, -1),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The "gid" argument is out of range'
  }
);
