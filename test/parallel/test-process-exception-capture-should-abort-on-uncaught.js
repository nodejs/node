'use strict';
const common = require('../common');

if (!process.execArgv.includes('--abort-on-uncaught-exception'))
  common.requireFlags(['--abort-on-uncaught-exception']);

const assert = require('assert');

assert.strictEqual(process.hasUncaughtExceptionCaptureCallback(), false);

// This should make the process not crash even though the flag was passed.
process.setUncaughtExceptionCaptureCallback(common.mustCall((err) => {
  assert.strictEqual(err.message, 'foo');
}));
throw new Error('foo');
