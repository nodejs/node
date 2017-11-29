'use strict';
const common = require('../common');
const assert = require('assert');

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

assert.doesNotThrow(() => require('domain'));
