'use strict';

// Stdin is not a TTY, we will read it and execute it.

const {
  initializeDeprecations,
  initializeClusterIPC,
  initializePolicy,
  initializeESMLoader,
  loadPreloadModules
} = require('internal/bootstrap/pre_execution');

const {
  evalScript,
  readStdin
} = require('internal/process/execution');

initializeDeprecations();
initializeClusterIPC();
initializePolicy();
initializeESMLoader();
loadPreloadModules();
markBootstrapComplete();

readStdin((code) => {
  process._eval = code;
  evalScript('[stdin]', process._eval, process._breakFirstLine);
});
