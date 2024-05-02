// Flags: --abort-on-uncaught-exception
'use strict';
const common = require('../common');
const assert = require('assert');

assert.strictEqual(process.hasUncaughtExceptionCaptureCallback(), false);

// This should make the process not crash even though the flag was passed.
process.setUncaughtExceptionCaptureCallback(common.mustCall((err) => {
  assert.strictEqual(err.message, 'foo');
}));
throw new Error('foo');
