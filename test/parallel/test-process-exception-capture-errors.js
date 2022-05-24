'use strict';
const common = require('../common');
const assert = require('assert');

assert.throws(
  () => process.setUncaughtExceptionCaptureCallback(42),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

assert.throws(
  () => process.setUncaughtExceptionCaptureCallback(common.mustNotCall()),
  {
    code: 'ERR_UNCAUGHT_EXCEPTION_CAPTURE_ALREADY_SET',
    name: 'Error',
  }
);
