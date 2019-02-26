'use strict';
const common = require('../common');

process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

require('domain'); // Should not throw.
