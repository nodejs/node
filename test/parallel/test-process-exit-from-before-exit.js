'use strict';
const common = require('../common');

process.on('beforeExit', common.mustCall(function() {
  setTimeout(common.mustNotCall(), 5);
  process.exit(0);  // Should execute immediately even if we schedule new work.
  common.fail();
}));
