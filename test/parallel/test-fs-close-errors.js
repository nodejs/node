'use strict';

const common = require('../common');
const fs = require('fs');

['', false, null, undefined, {}, []].forEach((i) => {
  common.expectsError(
    () => fs.close(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type number'
    }
  );
  common.expectsError(
    () => fs.closeSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type number'
    }
  );
});

[-1, 0xFFFFFFFF + 1].forEach((i) => {
  common.expectsError(
    () => fs.close(i),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The "fd" argument is out of range'
    }
  );
  common.expectsError(
    () => fs.closeSync(i),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The "fd" argument is out of range'
    }
  );
});
