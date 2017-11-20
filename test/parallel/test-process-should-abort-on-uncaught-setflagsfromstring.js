'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

assert.strictEqual(process.shouldAbortOnUncaughtException, true);

v8.setFlagsFromString('--abort-on-uncaught-exception');
// This should make the process not crash even though the flag was passed.
process.shouldAbortOnUncaughtException = false;
process.once('uncaughtException', common.mustCall());
throw new Error('foo');
