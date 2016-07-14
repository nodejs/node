'use strict';
var common = require('../common');

process.on('beforeExit', common.mustCall(function() {
  setTimeout(common.fail, 5);
  process.exit(0);  // Should execute immediately even if we schedule new work.
  common.fail();
}));
