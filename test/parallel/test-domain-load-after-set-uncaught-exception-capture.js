'use strict';
const common = require('../common');

process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

common.expectsError(
  () => require('domain'),
  {
    code: 'ERR_DOMAIN_CALLBACK_NOT_AVAILABLE',
    type: Error,
    message: /^A callback was registered.*with using the `domain` module/
  }
);

process.setUncaughtExceptionCaptureCallback(null);
require('domain'); // Should not throw.
