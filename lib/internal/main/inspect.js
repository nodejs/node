'use strict';

// `node inspect ...` or `node debug ...`

const {
  markBootstrapComplete,
  prepareMainThreadExecution
} = require('internal/process/pre_execution');

prepareMainThreadExecution();


markBootstrapComplete();

// Start the debugger agent.
process.nextTick(() => {
  require('internal/debugger/inspect').start();
});
