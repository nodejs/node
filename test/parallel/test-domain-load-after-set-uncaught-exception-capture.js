'use strict';
const common = require('../common');
const captureSym = Symbol('foo');

process.setUncaughtExceptionCaptureCallback(captureSym, common.mustNotCall());
require('domain'); // Should not throw.
