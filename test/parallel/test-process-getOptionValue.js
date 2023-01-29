'use strict';

// Flags: --pending-deprecation -C additional

require('../common');
const assert = require('node:assert');

// This test ensures that process.getOptionValue is exposed on process variable.

assert.strictEqual(process.getOptionValue('--pending-deprecation'), true);
assert.deepStrictEqual(process.getOptionValue('--conditions'), ['additional']);
