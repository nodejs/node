'use strict';

const common = require('../common');
const assert = require('node:assert');
const { getCallSites } = require('node:util');

// Asserts that util.getCallSites() does not invoke
// Error.prepareStackTrace.

Error.prepareStackTrace = common.mustNotCall();

const sites = getCallSites(1);
assert.strictEqual(sites.length, 1);
assert.strictEqual(sites[0].scriptName, __filename);
