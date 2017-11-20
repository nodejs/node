'use strict';
const common = require('../common');
const assert = require('assert');

// This value is ignored when the --abort-on-uncaught-exception flag is missing.
assert.strictEqual(process.shouldAbortOnUncaughtException, true);
process.once('uncaughtException', common.mustCall());
throw new Error('foo');
