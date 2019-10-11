'use strict';

const {
  prepareMainThreadExecution
} = require('internal/bootstrap/pre_execution');

prepareMainThreadExecution(true);

const runMainAsync = require('internal/main/main');

markBootstrapComplete();

const asyncMain = runMainAsync(process.argv[1]);

// Sync runMain. Async case hands over control to the promise chain.
// Calling runMain here ensures only this file is in stack for backward
// compatibility, instead of adding 'internal/main/main' to the stack.
if (!asyncMain) {
  const CJSModule = require('internal/modules/cjs/loader').Module;
  CJSModule.runMain(process.argv[1]);
}
