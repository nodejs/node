'use strict';
const common = require('../common');

common.expectsError(
  () => process.setUncaughtExceptionCaptureCallback(Symbol('foo'), 42),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "fn" argument must be one of type Function or null. ' +
             'Received type number'
  }
);

common.expectsError(
  () => process.setUncaughtExceptionCaptureCallback('foo', () => {}),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "owner" argument must be of type Symbol. Received type string'
  }
);
