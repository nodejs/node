'use strict';
// Tests that setUncaughtExceptionCaptureCallback can be called after domain
// is loaded. This verifies that the mutual exclusivity has been removed.
const common = require('../common');
const assert = require('assert');

// Load domain first
const domain = require('domain');
assert.ok(domain);

// Setting callback should not throw (coexistence is now supported)
process.setUncaughtExceptionCaptureCallback(common.mustNotCall());

// Verify callback is registered
assert.ok(process.hasUncaughtExceptionCaptureCallback());

// Clean up
process.setUncaughtExceptionCaptureCallback(null);
assert.ok(!process.hasUncaughtExceptionCaptureCallback());

// Domain should still be usable after callback operations
const d = domain.create();
assert.ok(d);
