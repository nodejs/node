'use strict';
const common = require('../common');

common.expectsError(
  () => process.setUncaughtExceptionCaptureCallback(42),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "fn" argument must be one of type Function or null. ' +
             'Received type number'
  }
);

process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

common.expectsError(
  () => process.setUncaughtExceptionCaptureCallback(common.mustNotCall()),
  {
    code: 'ERR_UNCAUGHT_EXCEPTION_CAPTURE_ALREADY_SET',
    type: Error,
    message: /setupUncaughtExceptionCapture.*called while a capture callback/
  }
);
