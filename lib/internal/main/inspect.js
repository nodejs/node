'use strict';

// `node inspect ...` or `node debug ...`

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');

prepareMainThreadExecution();


markBootstrapComplete();
require('internal/process/esm_loader').init();

// Start the debugger agent.
process.nextTick(() => {
  require('internal/debugger/inspect').start();
});
