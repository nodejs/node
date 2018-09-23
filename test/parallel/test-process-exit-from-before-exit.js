'use strict';
var assert = require('assert');
var common = require('../common');

process.on('beforeExit', common.mustCall(function() {
  setTimeout(assert.fail, 5);
  process.exit(0);  // Should execute immediately even if we schedule new work.
  assert.fail();
}));
