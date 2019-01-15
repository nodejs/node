'use strict';

// In worker threads, execute the script sent through the
// message port.

const {
  initializeClusterIPC,
  initializeESMLoader,
  loadPreloadModules
} = require('internal/bootstrap/pre_execution');

const {
  getEnvMessagePort,
  threadId
} = internalBinding('worker');

const {
  createMessageHandler,
  createWorkerFatalExeception
} = require('internal/process/worker_thread_only');

const debug = require('util').debuglog('worker');
debug(`[${threadId}] is setting up worker child environment`);

function prepareUserCodeExecution() {
  initializeClusterIPC();
  initializeESMLoader();
  loadPreloadModules();
}

// Set up the message port and start listening
const port = getEnvMessagePort();
port.on('message', createMessageHandler(port, prepareUserCodeExecution));
port.start();

// Overwrite fatalException
process._fatalException = createWorkerFatalExeception(port);

markBootstrapComplete();
