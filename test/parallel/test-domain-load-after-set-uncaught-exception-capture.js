'use strict';
// Tests that domain can be loaded after setUncaughtExceptionCaptureCallback
// has been called. This verifies that the mutual exclusivity has been removed.
const common = require('../common');

// Set up a capture callback first
process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

// Loading domain should not throw (coexistence is now supported)
const domain = require('domain');

// Verify domain module loaded successfully
const assert = require('assert');
assert.ok(domain);
assert.ok(domain.create);

// Clean up
process.setUncaughtExceptionCaptureCallback(null);

// Domain should still be usable
const d = domain.create();
assert.ok(d);
