'use strict';
const common = require('../common');
common.skipIfWorker();

// Test that 'exit' is emitted if 'beforeExit' throws.

process.on('exit', common.mustCall(() => {
  process.exitCode = 0;
}));
process.on('beforeExit', common.mustCall(() => {
  throw new Error();
}));
