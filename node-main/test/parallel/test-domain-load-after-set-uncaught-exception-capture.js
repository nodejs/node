'use strict';
const common = require('../common');
const assert = require('assert');

process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

assert.throws(
  () => require('domain'),
  {
    code: 'ERR_DOMAIN_CALLBACK_NOT_AVAILABLE',
    name: 'Error',
    message: /^A callback was registered.*with using the `domain` module/
  }
);

process.setUncaughtExceptionCaptureCallback(null);
require('domain'); // Should not throw.
