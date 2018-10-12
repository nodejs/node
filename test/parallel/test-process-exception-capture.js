// Flags: --abort-on-uncaught-exception
'use strict';
const common = require('../common');
const assert = require('assert');

const sym = Symbol('foo');

assert.strictEqual(process.hasUncaughtExceptionCaptureCallback(), false);

// This should make the process not crash even though the flag was passed.
process.setUncaughtExceptionCaptureCallback(sym, common.mustCall((err) => {
  assert.strictEqual(err.message, 'foo');
}));
process.on('uncaughtException', common.mustNotCall());
throw new Error('foo');
