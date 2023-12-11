'use strict';

// `node inspect ...` or `node debug ...`

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');
const { queueMicrotask } = require('internal/process/task_queues');

prepareMainThreadExecution();


markBootstrapComplete();

// Start the debugger agent.
queueMicrotask(() => {
  require('internal/debugger/inspect').start();
});
