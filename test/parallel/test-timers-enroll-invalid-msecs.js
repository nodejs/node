'use strict';

const common = require('../common');
const timers = require('timers');

[
  {},
  [],
  'foo',
  () => { },
  Symbol('foo')
].forEach((val) => {
  common.expectsError(
    () => timers.enroll({}, val),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

[
  -1,
  Infinity,
  NaN
].forEach((val) => {
  common.expectsError(
    () => timers.enroll({}, val),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "msecs" is out of range. ' +
               'It must be a non-negative finite number. ' +
               `Received ${val}`
    }
  );
});
