'use strict';

// `node inspect ...` or `node debug ...`

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

prepareMainThreadExecution();


markBootstrapComplete();

// Start the debugger agent.
process.nextTick(() => {
  require('internal/deps/node-inspect/lib/_inspect').start();
});
