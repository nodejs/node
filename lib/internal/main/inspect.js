'use strict';

// `node inspect ...` or `node debug ...`

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');

prepareMainThreadExecution();


markBootstrapComplete();

// Start the debugger agent.
process._deferTick(() => {
  require('internal/debugger/inspect').start();
});
