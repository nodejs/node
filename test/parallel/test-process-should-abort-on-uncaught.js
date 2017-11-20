// Flags: --abort-on-uncaught-exception
'use strict';
const common = require('../common');
const assert = require('assert');

assert.strictEqual(process.shouldAbortOnUncaughtException, true);

// This should make the process not crash even though the flag was passed.
process.shouldAbortOnUncaughtException = false;
process.once('uncaughtException', common.mustCall());
throw new Error('foo');
