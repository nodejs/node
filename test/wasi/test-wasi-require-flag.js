'use strict';
// This test verifies that the WASI module cannot be require()'ed without a
// CLI flag while it is still experimental.
require('../common');
const assert = require('assert');

assert.throws(() => {
  require('wasi');
}, /^Error: Cannot find module 'wasi'/);
